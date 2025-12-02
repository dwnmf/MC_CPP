#include "world.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

World::World(Shader* s, TextureManager* tm, Player* p) : shader(s), texture_manager(tm), player(p) {
#ifndef UNIT_TEST
    shader_daylight_loc = shader ? shader->find_uniform("u_Daylight") : -1;
    std::vector<unsigned int> indices;
    for (int i=0; i<CHUNK_WIDTH*CHUNK_HEIGHT*CHUNK_LENGTH*8; i++) {
        indices.insert(indices.end(), {4u*i, 4u*i+1, 4u*i+2, 4u*i+2, 4u*i+3, 4u*i});
    }
    glGenBuffers(1, &ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
}
World::~World() {
#ifndef UNIT_TEST
    if (ibo) glDeleteBuffers(1, &ibo);
#endif
    for(auto& kv : chunks) delete kv.second;
    if(save_system) delete save_system;
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
    }
    glm::ivec3 lp = get_local_pos(glm::vec3(pos));
    Chunk* c = chunks[cp];
    if(c->blocks[lp.x][lp.y][lp.z] == number) return;

    int old_sky_light = get_skylight(pos);

    c->blocks[lp.x][lp.y][lp.z] = number;
    c->modified = true;
    c->update_at_position(lp);

    bool now_opaque = (number != 0 && !block_types[number]->transparent);
    bool is_source = (number != 0 && std::find(light_blocks.begin(), light_blocks.end(), number) != light_blocks.end());

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
    glm::ivec3 cp = get_chunk_pos(glm::vec3(pos));
    // IMPORTANT: If chunk doesn't exist, it's open sky -> return 15
    if(chunks.find(cp)==chunks.end()) return 15;
    return chunks[cp]->get_sky_light(get_local_pos(glm::vec3(pos)));
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
void World::propagate_increase(bool update) {
    while(!light_increase_queue.empty()) {
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
void World::propagate_decrease(bool update) {
    while(!light_decrease_queue.empty()) {
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
void World::propagate_skylight_increase(bool update) {
    while(!skylight_increase_queue.empty()) {
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
void World::propagate_skylight_decrease(bool update) {
    while(!skylight_decrease_queue.empty()) {
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
void World::tick(float dt) {
    chunk_update_counter = 0; time++; pending_chunk_update_count = 0;
    if(time % 36000 == 0) incrementer = 1; if(time % 36000 == 18000) incrementer = -1; daylight += incrementer;
    if(!chunk_building_queue.empty()) { chunk_building_queue.front()->update_mesh(); chunk_building_queue.pop_front(); }
    for(auto* c : visible_chunks) c->process_chunk_updates();
}
void World::prepare_rendering() {
#ifdef UNIT_TEST
    return;
#endif
    visible_chunks.clear(); for(auto& kv : chunks) { if(player->check_in_frustum(kv.first)) visible_chunks.push_back(kv.second); }
    std::sort(visible_chunks.begin(), visible_chunks.end(), [&](Chunk* a, Chunk* b){ return glm::distance(player->position, a->position) > glm::distance(player->position, b->position); });
}
void World::draw() {
#ifdef UNIT_TEST
    return;
#endif
    float dm = daylight / 1800.0f; glClearColor(0.5 * (dm-0.26), 0.8*(dm-0.26), (dm-0.26)*1.36, 1.0);
    shader->setFloat(shader_daylight_loc, dm); glEnable(GL_CULL_FACE);
    for(auto* c : visible_chunks) c->draw(GL_TRIANGLES);
    draw_translucent();
}
void World::draw_translucent() {
#ifdef UNIT_TEST
    return;
#endif
    glDepthMask(GL_FALSE); glFrontFace(GL_CW); glEnable(GL_BLEND);
    for(auto* c : visible_chunks) c->draw_translucent(GL_TRIANGLES);
    glFrontFace(GL_CCW); for(auto* c : visible_chunks) c->draw_translucent(GL_TRIANGLES);
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
}
