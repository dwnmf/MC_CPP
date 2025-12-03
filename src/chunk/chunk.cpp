#include "chunk.h"
#include "../world.h"
#include "../options.h"
#include <cstring>
#include <algorithm>
#include <tuple> // Required for std::make_tuple

Chunk::Chunk(World* w, glm::ivec3 pos) : world(w), chunk_position(pos) {
    position = glm::vec3(pos.x * CHUNK_WIDTH, pos.y * CHUNK_HEIGHT, pos.z * CHUNK_LENGTH);
    memset(blocks, 0, sizeof(blocks));
    memset(lightmap, 0, sizeof(lightmap));

    for(int x=0; x<CHUNK_WIDTH/SUBCHUNK_WIDTH; x++)
        for(int y=0; y<CHUNK_HEIGHT/SUBCHUNK_HEIGHT; y++)
            for(int z=0; z<CHUNK_LENGTH/SUBCHUNK_LENGTH; z++)
                subchunks[std::make_tuple(x,y,z)] = new Subchunk(this, {x,y,z});

#ifndef UNIT_TEST
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Дадим драйверу минимум памяти, реальный размер будем выделять по факту в send_mesh_data_to_gpu
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

    size_t stride = 3 * sizeof(uint32_t);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, stride, (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, stride, (void*)(1*sizeof(uint32_t))); glEnableVertexAttribArray(1);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, stride, (void*)(2*sizeof(uint32_t))); glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world->ibo);
    shader_chunk_offset_loc = world->shader ? world->shader->find_uniform("u_ChunkPosition") : -1;
#endif
}

Chunk::~Chunk() {
#ifndef UNIT_TEST
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
#endif
    for(auto& kv : subchunks) delete kv.second;
}

int Chunk::get_block_light(glm::ivec3 pos) const { return lightmap[pos.x][pos.y][pos.z] & 0xF; }
void Chunk::set_block_light(glm::ivec3 pos, int v) { lightmap[pos.x][pos.y][pos.z] = (lightmap[pos.x][pos.y][pos.z] & 0xF0) | v; }
int Chunk::get_sky_light(glm::ivec3 pos) const { return (lightmap[pos.x][pos.y][pos.z] >> 4) & 0xF; }
void Chunk::set_sky_light(glm::ivec3 pos, int v) { lightmap[pos.x][pos.y][pos.z] = (lightmap[pos.x][pos.y][pos.z] & 0xF) | (v << 4); }
uint8_t Chunk::get_raw_light(glm::ivec3 pos) const { return lightmap[pos.x][pos.y][pos.z]; }

namespace {
int get_neighbor_index(const glm::ivec3& diff) {
    if (diff == Util::EAST) return 0;
    if (diff == Util::WEST) return 1;
    if (diff == Util::UP) return 2;
    if (diff == Util::DOWN) return 3;
    if (diff == Util::SOUTH) return 4;
    if (diff == Util::NORTH) return 5;
    return -1;
}
}

int Chunk::get_block_number_cached(glm::ivec3 global_pos) const {
    glm::ivec3 cp = world->get_chunk_pos(glm::vec3(global_pos));
    glm::ivec3 diff = cp - chunk_position;

    if (diff == glm::ivec3(0)) {
        glm::ivec3 lp = world->get_local_pos(glm::vec3(global_pos));
        return blocks[lp.x][lp.y][lp.z];
    }

    int idx = get_neighbor_index(diff);
    if (idx != -1 && neighbors[idx]) {
        glm::ivec3 lp = world->get_local_pos(glm::vec3(global_pos));
        return neighbors[idx]->blocks[lp.x][lp.y][lp.z];
    }

    return world->get_block_number(global_pos);
}

int Chunk::get_light_cached(glm::ivec3 global_pos) const {
    glm::ivec3 cp = world->get_chunk_pos(glm::vec3(global_pos));
    glm::ivec3 diff = cp - chunk_position;

    if (diff == glm::ivec3(0)) {
        glm::ivec3 lp = world->get_local_pos(glm::vec3(global_pos));
        return get_block_light(lp);
    }

    int idx = get_neighbor_index(diff);
    if (idx != -1 && neighbors[idx]) {
        glm::ivec3 lp = world->get_local_pos(glm::vec3(global_pos));
        return neighbors[idx]->get_block_light(lp);
    }

    return world->get_light(global_pos);
}

int Chunk::get_skylight_cached(glm::ivec3 global_pos) const {
    glm::ivec3 cp = world->get_chunk_pos(glm::vec3(global_pos));
    glm::ivec3 diff = cp - chunk_position;

    if (diff == glm::ivec3(0)) {
        glm::ivec3 lp = world->get_local_pos(glm::vec3(global_pos));
        return get_sky_light(lp);
    }

    int idx = get_neighbor_index(diff);
    if (idx != -1 && neighbors[idx]) {
        glm::ivec3 lp = world->get_local_pos(glm::vec3(global_pos));
        return neighbors[idx]->get_sky_light(lp);
    }

    return world->get_skylight(global_pos);
}

bool Chunk::is_opaque_cached(glm::ivec3 global_pos) const {
    int n = get_block_number_cached(global_pos);
    if (!n) return false;
    return !world->block_types[n]->transparent;
}

void Chunk::update_subchunk_meshes() {
    chunk_update_queue.clear();
    for(auto& kv : subchunks) chunk_update_queue.push_back(kv.second);
}

// FIX: Robust update logic matching Python mcpy
void Chunk::update_at_position(glm::ivec3 pos) {
    int x = pos.x; int y = pos.y; int z = pos.z;

    int lx = x % SUBCHUNK_WIDTH;
    int ly = y % SUBCHUNK_HEIGHT;
    int lz = z % SUBCHUNK_LENGTH;

    int sx = x / SUBCHUNK_WIDTH;
    int sy = y / SUBCHUNK_HEIGHT;
    int sz = z / SUBCHUNK_LENGTH;

    auto add_sc = [&](int _sx, int _sy, int _sz) {
        auto key = std::make_tuple(_sx, _sy, _sz);
        if(subchunks.count(key)) {
            auto* sc = subchunks[key];
            bool found = false;
            for(auto* s : chunk_update_queue) if(s==sc) found=true;
            if(!found) chunk_update_queue.push_back(sc);
        }
    };

    add_sc(sx, sy, sz); // Update current subchunk

    // Update neighbor subchunks if on border
    if (lx == SUBCHUNK_WIDTH - 1) add_sc(sx + 1, sy, sz);
    if (lx == 0) add_sc(sx - 1, sy, sz);

    if (ly == SUBCHUNK_HEIGHT - 1) add_sc(sx, sy + 1, sz);
    if (ly == 0) add_sc(sx, sy - 1, sz);

    if (lz == SUBCHUNK_LENGTH - 1) add_sc(sx, sy, sz + 1);
    if (lz == 0) add_sc(sx, sy, sz - 1);
}

void Chunk::process_chunk_updates() {
    for (int i=0; i < Options::CHUNK_UPDATES; i++) {
        if(chunk_update_queue.empty()) break;
        Subchunk* sc = chunk_update_queue.front();
        chunk_update_queue.pop_front();
        sc->update_mesh();
        world->chunk_update_counter++;
        if(chunk_update_queue.empty()) world->chunk_building_queue.push_back(this);
    }
}

void Chunk::update_mesh() {
    mesh.clear(); translucent_mesh.clear();
    size_t mesh_total = 0;
    size_t translucent_total = 0;
    for (auto& kv : subchunks) {
        mesh_total += kv.second->mesh.size();
        translucent_total += kv.second->translucent_mesh.size();
    }
    if (mesh_total) mesh.reserve(mesh_total);
    if (translucent_total) translucent_mesh.reserve(translucent_total);

    for(auto& kv : subchunks) {
        if(!kv.second->mesh.empty()) mesh.insert(mesh.end(), kv.second->mesh.begin(), kv.second->mesh.end());
        if(!kv.second->translucent_mesh.empty()) translucent_mesh.insert(translucent_mesh.end(), kv.second->translucent_mesh.begin(), kv.second->translucent_mesh.end());
    }
    mesh_quad_count = mesh.size() / 12; // 3 uint32 per vertex * 4 vertices
    translucent_quad_count = translucent_mesh.size() / 12;
    send_mesh_data_to_gpu();
}

void Chunk::send_mesh_data_to_gpu() {
#ifdef UNIT_TEST
    return;
#endif
    size_t required_ints = mesh.size() + translucent_mesh.size();
    if(!required_ints) { vbo_capacity = 0; return; }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if(required_ints != vbo_capacity) {
        glBufferData(GL_ARRAY_BUFFER, sizeof(uint32_t)*required_ints, NULL, GL_DYNAMIC_DRAW);
        vbo_capacity = required_ints;
    }
    if(!mesh.empty()) glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(uint32_t)*mesh.size(), mesh.data());
    if(!translucent_mesh.empty()) glBufferSubData(GL_ARRAY_BUFFER, sizeof(uint32_t)*mesh.size(), sizeof(uint32_t)*translucent_mesh.size(), translucent_mesh.data());
}

void Chunk::draw(GLenum mode) {
#ifdef UNIT_TEST
    return;
#endif
    if(!mesh_quad_count) return;
    glBindVertexArray(vao);
    world->shader->setVec2i(shader_chunk_offset_loc, chunk_position.x, chunk_position.z);
    glDrawElements(mode, mesh_quad_count * 6, GL_UNSIGNED_INT, 0);
}

void Chunk::draw_translucent(GLenum mode) {
#ifdef UNIT_TEST
    return;
#endif
    if(!translucent_quad_count) return;
    glBindVertexArray(vao);
    world->shader->setVec2i(shader_chunk_offset_loc, chunk_position.x, chunk_position.z);
    glDrawElementsBaseVertex(mode, translucent_quad_count * 6, GL_UNSIGNED_INT, 0, mesh_quad_count * 4);
}
