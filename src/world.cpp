#include "world.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <utility>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <limits>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

World::World(Shader* s, TextureManager* tm, Player* p) : shader(s), texture_manager(tm), player(p) {
#ifndef UNIT_TEST
    shader_daylight_loc = -1;
    if (shader && shader->valid()) {
        shader->use();
        shader_daylight_loc = shader->find_uniform("u_Daylight");
        // Ensure shadows are disabled in the main shader by default
        shader_cascade_count_loc = shader->find_uniform("u_ShadowCascadeCount");
        if (shader_cascade_count_loc >= 0) shader->setInt(shader_cascade_count_loc, 0);
    }

    std::vector<unsigned int> indices;
    for (int i=0; i<CHUNK_WIDTH*CHUNK_HEIGHT*CHUNK_LENGTH*8; i++) {
        indices.insert(indices.end(), {4u*i, 4u*i+1, 4u*i+2, 4u*i+2, 4u*i+3, 4u*i});
    }
    glGenBuffers(1, &ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    shadows_enabled = Options::SHADOWS_ENABLED;
    if (shadows_enabled && shader && shader->valid() && texture_manager) {
        if (!init_shadow_resources()) {
            shadows_enabled = false;
        }
    }
#endif
}
World::~World() {
#ifndef UNIT_TEST
    if (ibo) glDeleteBuffers(1, &ibo);
#endif
    for(auto& kv : chunks) delete kv.second;
    if(save_system) delete save_system;
#ifndef UNIT_TEST
    if (shadow_fbo) glDeleteFramebuffers(1, &shadow_fbo);
    if (shadow_map) glDeleteTextures(1, &shadow_map);
    if (shadow_shader) delete shadow_shader;
#endif
}
glm::ivec3 World::get_chunk_pos(glm::vec3 pos) { return glm::ivec3(floor(pos.x/16), floor(pos.y/128), floor(pos.z/16)); }
glm::ivec3 World::get_local_pos(glm::vec3 pos) {
    int x = (int)floor(pos.x) % 16; if(x<0) x+=16;
    int y = (int)floor(pos.y) % 128; if(y<0) y+=128;
    int z = (int)floor(pos.z) % 16; if(z<0) z+=16;
    return glm::ivec3(x,y,z);
}
int World::get_block_number(glm::ivec3 pos) {
    glm::ivec3 cp = get_chunk_pos(glm::vec3(pos));
    if(chunks.find(cp) == chunks.end()) return 0;
    glm::ivec3 lp = get_local_pos(glm::vec3(pos));
    return chunks[cp]->blocks[lp.x][lp.y][lp.z];
}

void World::set_block(glm::ivec3 pos, int number) {
    if (pos.y < 0 || pos.y >= CHUNK_HEIGHT) return;
    glm::ivec3 cp = get_chunk_pos(glm::vec3(pos));
    if(chunks.find(cp) == chunks.end()) {
        if(number == 0) return;
        chunks[cp] = new Chunk(this, cp);
        init_skylight(chunks[cp]);
        stitch_sky_light(chunks[cp]);
        stitch_block_light(chunks[cp]);
    }
    glm::ivec3 lp = get_local_pos(glm::vec3(pos));
    Chunk* c = chunks[cp];
    if(c->blocks[lp.x][lp.y][lp.z] == number) return;

    int old_sky_light = get_skylight(pos);

    c->blocks[lp.x][lp.y][lp.z] = number;
    c->modified = true;
    c->update_at_position(lp);

    bool now_opaque = (number != 0 && !block_types[number]->transparent);
    bool is_source = (number != 0 && light_blocks.find(number) != light_blocks.end());

    // 1. Block Light
    if (is_source) {
        increase_light(pos, 15);
    } else if (get_light(pos) > 0) {
        decrease_light(pos);
    }
    // Если блок сломали, свет от соседей должен заполнить пустоту
    else if (number == 0 || block_types[number]->transparent) {
        for(auto& d : Util::DIRECTIONS) {
            glm::ivec3 n = pos + d;
            int l = get_light(n);
            if (l > 0) light_increase_queue.push_back({n, l});
        }
        propagate_increase(true);
    }

    // 2. Sky Light
    if (now_opaque && old_sky_light == 15) {
        // Optimization: Blocked the sun. Strip column down.
        decrease_skylight(pos);
        for (int y = pos.y - 1; y >= 0; y--) {
            glm::ivec3 downPos(pos.x, y, pos.z);
            if (is_opaque_block(downPos)) break;

            chunks[cp]->set_sky_light(get_local_pos(glm::vec3(downPos)), 0);
            chunks[cp]->update_at_position(get_local_pos(glm::vec3(downPos)));
            skylight_decrease_queue.push_back({downPos, 15});
        }
        propagate_skylight_decrease(true);
        propagate_skylight_increase(true);
    } else {
        if (now_opaque) {
            decrease_skylight(pos);
        } else {
            // Block broken: Sun/Light should flood in.
            // Add all neighbors to the queue as sources.
            for(auto& d : Util::DIRECTIONS) {
                glm::ivec3 n = pos + d;

                // Special case: light coming from above the world height
                if (n.y >= CHUNK_HEIGHT) {
                    skylight_increase_queue.push_back({n, 15});
                } else {
                    int l = get_skylight(n);
                    if (l > 0) skylight_increase_queue.push_back({n, l});
                }
            }
            propagate_skylight_increase(true);
        }
    }

    if(lp.x==0 && chunks.count(cp+Util::WEST)) chunks[cp+Util::WEST]->update_at_position(lp+glm::ivec3(15,0,0));
    if(lp.x==15 && chunks.count(cp+Util::EAST)) chunks[cp+Util::EAST]->update_at_position(lp-glm::ivec3(15,0,0));
    if(lp.y==0 && chunks.count(cp+Util::DOWN)) chunks[cp+Util::DOWN]->update_at_position(lp+glm::ivec3(0,127,0));
    if(lp.y==127 && chunks.count(cp+Util::UP)) chunks[cp+Util::UP]->update_at_position(lp-glm::ivec3(0,127,0));
    if(lp.z==0 && chunks.count(cp+Util::NORTH)) chunks[cp+Util::NORTH]->update_at_position(lp+glm::ivec3(0,0,15));
    if(lp.z==15 && chunks.count(cp+Util::SOUTH)) chunks[cp+Util::SOUTH]->update_at_position(lp-glm::ivec3(0,0,15));
}

bool World::try_set_block(glm::ivec3 pos, int number, const Collider& player_collider) {
    if (pos.y < 0 || pos.y >= CHUNK_HEIGHT) return false;
    if (number == 0) { set_block(pos, 0); return true; }
    if (number < block_types.size() && block_types[number]) {
        for (const auto& block_col : block_types[number]->colliders) {
            Collider world_col = block_col + glm::vec3(pos);
            if (world_col & player_collider) return false;
        }
    }
    set_block(pos, number);
    return true;
}

int World::get_light(glm::ivec3 pos) {
    glm::ivec3 cp = get_chunk_pos(glm::vec3(pos)); if(chunks.find(cp)==chunks.end()) return 0;
    return chunks[cp]->get_block_light(get_local_pos(glm::vec3(pos)));
}
int World::get_skylight(glm::ivec3 pos) {
    if (pos.y < 0) return 0;
    if (pos.y >= CHUNK_HEIGHT) return 15;
    glm::ivec3 cp = get_chunk_pos(glm::vec3(pos));
    auto it = chunks.find(cp);
    // Treat missing chunks as dark to avoid leaking skylight through unloaded neighbors
    if (it == chunks.end()) return 0;
    return it->second->get_sky_light(get_local_pos(glm::vec3(pos)));
}
bool World::is_opaque_block(glm::ivec3 pos) {
    int n = get_block_number(pos); if(!n) return false; return !block_types[n]->transparent;
}
bool World::get_transparency(glm::ivec3 pos) {
    int n = get_block_number(pos); if(!n) return true; return block_types[n]->transparent;
}

void World::increase_light(glm::ivec3 pos, int val, bool update) {
    auto cp = get_chunk_pos(glm::vec3(pos)); if(chunks.find(cp)==chunks.end()) return;
    chunks[cp]->set_block_light(get_local_pos(glm::vec3(pos)), val);
    light_increase_queue.push_back({pos, val}); propagate_increase(update);
}
void World::propagate_increase(bool update, int max_steps) {
    int steps_left = (max_steps < 0) ? Options::LIGHT_STEPS_PER_TICK : max_steps;
    if (steps_left <= 0) steps_left = std::numeric_limits<int>::max();
    while(!light_increase_queue.empty() && steps_left-- > 0) {
        auto [pos, level] = light_increase_queue.front(); light_increase_queue.pop_front();
        for(auto& d : Util::DIRECTIONS) {
            glm::ivec3 n = pos + d; glm::ivec3 cp = get_chunk_pos(glm::vec3(n));
            if(chunks.find(cp)==chunks.end()) continue;
            int l = get_light(n);
            if(!is_opaque_block(n) && l + 2 <= level) {
                chunks[cp]->set_block_light(get_local_pos(glm::vec3(n)), level-1);
                light_increase_queue.push_back({n, level-1});
                if(update) chunks[cp]->update_at_position(get_local_pos(glm::vec3(n)));
            }
        }
    }
}
void World::decrease_light(glm::ivec3 pos) {
    int old = get_light(pos); chunks[get_chunk_pos(glm::vec3(pos))]->set_block_light(get_local_pos(glm::vec3(pos)), 0);
    light_decrease_queue.push_back({pos, old}); propagate_decrease(true); propagate_increase(true);
}
void World::propagate_decrease(bool update, int max_steps) {
    int steps_left = (max_steps < 0) ? Options::LIGHT_STEPS_PER_TICK : max_steps;
    if (steps_left <= 0) steps_left = std::numeric_limits<int>::max();
    while(!light_decrease_queue.empty() && steps_left-- > 0) {
        auto [pos, level] = light_decrease_queue.front(); light_decrease_queue.pop_front();
        for(auto& d : Util::DIRECTIONS) {
            glm::ivec3 n = pos + d; if(chunks.find(get_chunk_pos(glm::vec3(n))) == chunks.end()) continue;
            int nl = get_light(n); if(nl == 0) continue;
            if(nl < level) {
                chunks[get_chunk_pos(glm::vec3(n))]->set_block_light(get_local_pos(glm::vec3(n)), 0);
                if(update) chunks[get_chunk_pos(glm::vec3(n))]->update_at_position(get_local_pos(glm::vec3(n)));
                light_decrease_queue.push_back({n, nl});
            } else if(nl >= level) { light_increase_queue.push_back({n, nl}); }
        }
    }
}

// === SKYLIGHT INITIALIZATION ===
void World::init_skylight(Chunk* c) {
    glm::ivec3 cp = c->chunk_position;
    glm::ivec3 global_base = cp * glm::ivec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_LENGTH);

    // 1. Vertical Pass
    for(int x=0; x<CHUNK_WIDTH; x++) {
        for(int z=0; z<CHUNK_LENGTH; z++) {
            int height = -1;
            for(int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
                if(c->blocks[x][y][z] != 0 && is_opaque_block({global_base.x+x, y, global_base.z+z})) {
                    height = y;
                    break;
                }
            }
            // Fill sun above
            for(int y = CHUNK_HEIGHT - 1; y > height; y--) {
                c->set_sky_light({x, y, z}, 15);
                skylight_increase_queue.push_back({glm::ivec3(global_base.x+x, y, global_base.z+z), 15});
            }
            // Fill darkness below
            for(int y = height; y >= 0; y--) {
                c->set_sky_light({x, y, z}, 0);
            }
        }
    }

    // 2. Border Pass
    auto check_border_neighbor = [&](glm::ivec3 local_pos, glm::ivec3 global_neighbor_pos) {
        if (c->blocks[local_pos.x][local_pos.y][local_pos.z] != 0 &&
            !block_types[c->blocks[local_pos.x][local_pos.y][local_pos.z]]->transparent) return;

        int neighbor_light = get_skylight(global_neighbor_pos);
        int current_light = c->get_sky_light(local_pos);
        int decay = 1;

        if (neighbor_light - decay > current_light) {
            int new_val = neighbor_light - decay;
            c->set_sky_light(local_pos, new_val);
            skylight_increase_queue.push_back({global_base + local_pos, new_val});
        }
    };

    for(int y=0; y<CHUNK_HEIGHT; y++) {
        for(int z=0; z<CHUNK_LENGTH; z++) {
            check_border_neighbor({0, y, z}, {global_base.x - 1, y, global_base.z + z});
            check_border_neighbor({15, y, z}, {global_base.x + 16, y, global_base.z + z});
        }
        for(int x=0; x<CHUNK_WIDTH; x++) {
            check_border_neighbor({x, y, 0}, {global_base.x + x, y, global_base.z - 1});
            check_border_neighbor({x, y, 15}, {global_base.x + x, y, global_base.z + 16});
        }
    }
}

void World::decrease_skylight(glm::ivec3 pos) {
    int old = get_skylight(pos); chunks[get_chunk_pos(glm::vec3(pos))]->set_sky_light(get_local_pos(glm::vec3(pos)), 0);
    skylight_decrease_queue.push_back({pos, old}); propagate_skylight_decrease(true); propagate_skylight_increase(true);
}
void World::propagate_skylight_increase(bool update, int max_steps) {
    int steps_left = (max_steps < 0) ? Options::LIGHT_STEPS_PER_TICK : max_steps;
    if (steps_left <= 0) steps_left = std::numeric_limits<int>::max();
    while(!skylight_increase_queue.empty() && steps_left-- > 0) {
        auto [pos, level] = skylight_increase_queue.front(); skylight_increase_queue.pop_front();
        for(auto& d : Util::DIRECTIONS) {
            glm::ivec3 n = pos + d; if(n.y >= CHUNK_HEIGHT || n.y < 0) continue;
            if(chunks.find(get_chunk_pos(glm::vec3(n))) == chunks.end()) continue;

            int block_id = get_block_number(n);
            int decay = 1;
            if (d.y == -1) {
                decay = 0;
                if (block_id != 0 && !block_types[block_id]->glass) decay = 1;
            }

            if(!is_opaque_block(n)) {
                int nl = get_skylight(n); int new_l = level - decay;
                if (new_l > nl && new_l > 0) {
                    chunks[get_chunk_pos(glm::vec3(n))]->set_sky_light(get_local_pos(glm::vec3(n)), new_l);
                    skylight_increase_queue.push_back({n, new_l});
                    if(update) chunks[get_chunk_pos(glm::vec3(n))]->update_at_position(get_local_pos(glm::vec3(n)));
                }
            }
        }
    }
}
void World::propagate_skylight_decrease(bool update, int max_steps) {
    int steps_left = (max_steps < 0) ? Options::LIGHT_STEPS_PER_TICK : max_steps;
    if (steps_left <= 0) steps_left = std::numeric_limits<int>::max();
    while(!skylight_decrease_queue.empty() && steps_left-- > 0) {
        auto [pos, level] = skylight_decrease_queue.front(); skylight_decrease_queue.pop_front();
        for(auto& d : Util::DIRECTIONS) {
            glm::ivec3 n = pos + d; if(chunks.find(get_chunk_pos(glm::vec3(n))) == chunks.end()) continue;
            int nl = get_skylight(n); if(nl == 0) continue;

            if(d.y == -1 || nl < level) {
                chunks[get_chunk_pos(glm::vec3(n))]->set_sky_light(get_local_pos(glm::vec3(n)), 0);
                if(update) chunks[get_chunk_pos(glm::vec3(n))]->update_at_position(get_local_pos(glm::vec3(n)));
                skylight_decrease_queue.push_back({n, nl});
            } else if(nl >= level) {
                skylight_increase_queue.push_back({n, nl});
            }
        }
    }
}
void World::speed_daytime() { if(daylight <= 480) incrementer = 1; if(daylight >= 1800) incrementer = -1; }
glm::vec3 World::get_light_direction() const {
    double phase = std::fmod(static_cast<double>(time) + 9000.0, 36000.0) / 36000.0;
    double azimuth = phase * glm::two_pi<double>();
    float elevation = glm::mix(0.2f, 0.85f, static_cast<float>(0.5 * (std::sin(azimuth) + 1.0)));
    return glm::normalize(glm::vec3(static_cast<float>(std::cos(azimuth)), -elevation, static_cast<float>(std::sin(azimuth))));
}
float World::get_daylight_factor() const {
    return std::clamp(daylight / 1800.0f, 0.0f, 1.0f);
}
void World::tick(float dt) {
    chunk_update_counter = 0; time++; pending_chunk_update_count = 0;

    // Day/night cycle: smooth sinusoidal between dawn and noon values
    double phase = std::fmod(static_cast<double>(time) + 9000.0, 36000.0) / 36000.0;
    float sun_height = static_cast<float>(0.5 * (std::sin(phase * glm::two_pi<double>()) + 1.0));
    daylight = glm::mix(480.0f, 1800.0f, sun_height);

    if(!chunk_building_queue.empty()) { chunk_building_queue.front()->update_mesh(); chunk_building_queue.pop_front(); }
    for(auto* c : visible_chunks) c->process_chunk_updates();
    propagate_increase(true);
    propagate_decrease(true);
    propagate_skylight_increase(true);
    propagate_skylight_decrease(true);
}

void World::stitch_block_light(Chunk* c) {
    glm::ivec3 base = c->chunk_position * glm::ivec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_LENGTH);
    auto consider = [&](int lx, int ly, int lz, glm::ivec3 dir) {
        glm::ivec3 global = base + glm::ivec3(lx, ly, lz);
        glm::ivec3 npos = global + dir;
        int neighbor_light = get_light(npos);
        if (neighbor_light <= 0) return;
        if (is_opaque_block(global)) return;
        int candidate = neighbor_light - 1;
        int current = c->get_block_light({lx, ly, lz});
        if (candidate > current) {
            c->set_block_light({lx, ly, lz}, candidate);
            light_increase_queue.push_back({global, candidate});
            c->update_at_position({lx, ly, lz});
        }
    };

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_LENGTH; z++) {
            consider(0, y, z, Util::WEST);
            consider(CHUNK_WIDTH - 1, y, z, Util::EAST);
        }
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            consider(x, y, 0, Util::NORTH);
            consider(x, y, CHUNK_LENGTH - 1, Util::SOUTH);
        }
    }
}

void World::stitch_sky_light(Chunk* c) {
    glm::ivec3 base = c->chunk_position * glm::ivec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_LENGTH);
    auto consider = [&](int lx, int ly, int lz, glm::ivec3 dir) {
        glm::ivec3 global = base + glm::ivec3(lx, ly, lz);
        glm::ivec3 npos = global + dir;
        int neighbor_light = get_skylight(npos);
        if (neighbor_light <= 0) return;
        if (is_opaque_block(global)) return;
        int decay = (dir.y == -1) ? 0 : 1;
        int candidate = neighbor_light - decay;
        int current = c->get_sky_light({lx, ly, lz});
        if (candidate > current) {
            c->set_sky_light({lx, ly, lz}, candidate);
            skylight_increase_queue.push_back({global, candidate});
            c->update_at_position({lx, ly, lz});
        }
    };

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_LENGTH; z++) {
            consider(0, y, z, Util::WEST);
            consider(CHUNK_WIDTH - 1, y, z, Util::EAST);
        }
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            consider(x, y, 0, Util::NORTH);
            consider(x, y, CHUNK_LENGTH - 1, Util::SOUTH);
        }
    }
}
void World::prepare_rendering() {
#ifdef UNIT_TEST
    return;
#endif
    visible_chunks.clear();
    std::vector<std::pair<float, Chunk*>> candidates;
    candidates.reserve(chunks.size());
    glm::vec3 player_pos = player->position;
    for(auto& kv : chunks) {
        glm::vec3 center = kv.second->position + glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_LENGTH * 0.5f);
        float dist2 = glm::length2(player_pos - center);
        candidates.push_back({dist2, kv.second});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){ return a.first > b.first; });
    visible_chunks.reserve(candidates.size());
    for (auto& c : candidates) visible_chunks.push_back(c.second);
}

bool World::init_shadow_resources() {
#ifdef UNIT_TEST
    return false;
#endif
    shadow_map_resolution = Options::SHADOW_MAP_RESOLUTION;
    shadow_cascade_count = std::max(1, std::min(Options::SHADOW_CASCADES, 4));

    if (shadow_map_resolution <= 0 || shadow_cascade_count <= 0) return false;

    shadow_matrices.resize(shadow_cascade_count);
    shadow_splits.resize(shadow_cascade_count);

    shadow_shader = new Shader("assets/shaders/shadow/vert.glsl", "assets/shaders/shadow/frag.glsl");
    if (!shadow_shader->valid()) {
        delete shadow_shader;
        shadow_shader = nullptr;
        std::cout << "ERROR::SHADOW:: Failed to load shadow shader." << std::endl;
        return false;
    }

    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);

    glGenTextures(1, &shadow_map);
    glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
                 shadow_map_resolution, shadow_map_resolution, shadow_cascade_count,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR::SHADOW:: Framebuffer is not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &shadow_fbo); shadow_fbo = 0;
        glDeleteTextures(1, &shadow_map); shadow_map = 0;
        delete shadow_shader; shadow_shader = nullptr;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Cache uniform locations on the main shader
    if (shader && shader->valid()) {
        shader_shadow_map_loc = shader->find_uniform("u_ShadowMap");
        shader_light_space_mats_loc = shader->find_uniform("u_LightSpaceMatrices[0]");
        shader_cascade_splits_loc = shader->find_uniform("u_CascadeSplits");
        shader_cascade_count_loc = shader->find_uniform("u_ShadowCascadeCount");
        shader_shadow_texel_size_loc = shader->find_uniform("u_ShadowTexelSize");
        shader_shadow_min_bias_loc = shader->find_uniform("u_ShadowMinBias");
        shader_shadow_slope_bias_loc = shader->find_uniform("u_ShadowSlopeBias");
        shader_shadow_pcf_radius_loc = shader->find_uniform("u_ShadowPCFRadius");
    }

    return true;
}

void World::update_shadow_cascades() {
#ifdef UNIT_TEST
    return;
#endif
    if (!shadows_enabled || !shadow_shader || !player) return;
    if (shadow_cascade_count <= 0) return;

    float near_plane = player->near_plane;
    float far_plane = player->far_plane;
    if (near_plane <= 0.0f || far_plane <= near_plane) return;

    float aspect = (player->view_height > 0.0f) ? (player->view_width / player->view_height) : 1.0f;
    float fov_rad = glm::radians(player->get_current_fov());
    float tanHalfFovY = std::tan(fov_rad * 0.5f);
    float tanHalfFovX = tanHalfFovY * aspect;

    float n = near_plane;
    float f = far_plane;
    float lambda = std::clamp(Options::SHADOW_LOG_WEIGHT, 0.0f, 1.0f);

    for (int i = 0; i < shadow_cascade_count; ++i) {
        float p = (i + 1) / static_cast<float>(shadow_cascade_count);
        float logSplit = n * std::pow(f / n, p);
        float linSplit = n + (f - n) * p;
        float splitDist = lambda * logSplit + (1.0f - lambda) * linSplit;
        shadow_splits[i] = splitDist;
    }

    glm::mat4 invView = glm::inverse(player->mv_matrix);
    glm::vec3 lightDir = glm::normalize(get_light_direction());

    for (int i = 0; i < shadow_cascade_count; ++i) {
        float prevSplit = (i == 0) ? near_plane : shadow_splits[i - 1];
        float splitDist = shadow_splits[i];

        float xn = prevSplit * tanHalfFovX;
        float yn = prevSplit * tanHalfFovY;
        float xf = splitDist * tanHalfFovX;
        float yf = splitDist * tanHalfFovY;

        glm::vec3 cornersView[8] = {
            { -xn,  yn, -prevSplit },
            {  xn,  yn, -prevSplit },
            { -xn, -yn, -prevSplit },
            {  xn, -yn, -prevSplit },
            { -xf,  yf, -splitDist },
            {  xf,  yf, -splitDist },
            { -xf, -yf, -splitDist },
            {  xf, -yf, -splitDist }
        };

        glm::vec3 cornersWorld[8];
        glm::vec3 center(0.0f);
        for (int c = 0; c < 8; ++c) {
            glm::vec4 worldPos = invView * glm::vec4(cornersView[c], 1.0f);
            cornersWorld[c] = glm::vec3(worldPos);
            center += cornersWorld[c];
        }
        center /= 8.0f;

        float radius = 0.0f;
        for (int c = 0; c < 8; ++c) {
            radius = std::max(radius, glm::length(cornersWorld[c] - center));
        }
        radius = std::max(radius, 1.0f);
        radius = std::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 lightPos = center - lightDir * (radius * 2.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

        float zNear = 0.1f;
        float zFar = radius * 4.0f;
        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, zNear, zFar);

        shadow_matrices[i] = lightProj * lightView;
    }
}

void World::render_shadows() {
#ifdef UNIT_TEST
    return;
#endif
    if (!shadows_enabled || !shadow_shader || !texture_manager) return;
    if (shadow_cascade_count <= 0) return;
    if (visible_chunks.empty()) return;

    update_shadow_cascades();

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint prevDrawBuffer = GL_BACK;
    GLint prevReadBuffer = GL_BACK;
    glGetIntegerv(GL_DRAW_BUFFER, &prevDrawBuffer);
    glGetIntegerv(GL_READ_BUFFER, &prevReadBuffer);

    glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);

    shadow_shader->use();
    int chunkLoc = shadow_shader->find_uniform("u_ChunkPosition");
    int lightSpaceLoc = shadow_shader->find_uniform("u_LightSpaceMatrix");
    int samplerLoc = shadow_shader->find_uniform("u_TextureArraySampler");
    if (samplerLoc >= 0) shadow_shader->setInt(samplerLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_manager->texture_array);

    for (int i = 0; i < shadow_cascade_count; ++i) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map, 0, i);
        glClear(GL_DEPTH_BUFFER_BIT);

        if (lightSpaceLoc >= 0) shadow_shader->setMat4(lightSpaceLoc, shadow_matrices[i]);

        for (auto* c : visible_chunks) {
            c->draw(GL_TRIANGLES, shadow_shader, chunkLoc);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(prevDrawBuffer);
    glReadBuffer(prevReadBuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void World::draw() {
#ifdef UNIT_TEST
    return;
#endif
    float dm = get_daylight_factor();
    glClearColor(0.5f * (dm-0.26f), 0.8f*(dm-0.26f), (dm-0.26f)*1.36f, 1.0f);
    if (shader && shader->valid()) {
        shader->use();
        shader->setFloat(shader_daylight_loc, dm);
        // Гарантируем, что сэмплер текстур мира всегда указывает на GL_TEXTURE0
        int texLoc = shader->find_uniform("u_TextureArraySampler");
        if (texLoc >= 0) shader->setInt(texLoc, 0);

        if (shadows_enabled && shadow_map && shadow_cascade_count > 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map);

            if (shader_shadow_map_loc >= 0) shader->setInt(shader_shadow_map_loc, 1);
            if (shader_light_space_mats_loc >= 0) shader->setMat4Array(shader_light_space_mats_loc, shadow_matrices);
            if (shader_cascade_splits_loc >= 0) shader->setFloatArray(shader_cascade_splits_loc, shadow_splits);
            if (shader_cascade_count_loc >= 0) shader->setInt(shader_cascade_count_loc, shadow_cascade_count);
            if (shader_shadow_texel_size_loc >= 0 && shadow_map_resolution > 0) {
                glm::vec2 texelSize(1.0f / static_cast<float>(shadow_map_resolution),
                                    1.0f / static_cast<float>(shadow_map_resolution));
                shader->setVec2(shader_shadow_texel_size_loc, texelSize);
            }
            if (shader_shadow_min_bias_loc >= 0) shader->setFloat(shader_shadow_min_bias_loc, Options::SHADOW_MIN_BIAS);
            if (shader_shadow_slope_bias_loc >= 0) shader->setFloat(shader_shadow_slope_bias_loc, Options::SHADOW_SLOPE_BIAS);
            if (shader_shadow_pcf_radius_loc >= 0) shader->setInt(shader_shadow_pcf_radius_loc, Options::SHADOW_PCF_RADIUS);
        } else if (shader_cascade_count_loc >= 0) {
            // Explicitly disable shadows in shader if resources are missing
            shader->setInt(shader_cascade_count_loc, 0);
        }
    }

    glEnable(GL_CULL_FACE);
    for(auto* c : visible_chunks) c->draw(GL_TRIANGLES);
    draw_translucent();
}
void World::draw_translucent() {
#ifdef UNIT_TEST
    return;
#endif
    glDepthMask(GL_FALSE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    for(auto* c : visible_chunks) c->draw_translucent(GL_TRIANGLES);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}
