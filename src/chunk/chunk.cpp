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

    size_t stride = 9 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7*sizeof(float))); glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8*sizeof(float))); glEnableVertexAttribArray(4);

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

int Chunk::get_block_light(glm::ivec3 pos) { return lightmap[pos.x][pos.y][pos.z] & 0xF; }
void Chunk::set_block_light(glm::ivec3 pos, int v) { lightmap[pos.x][pos.y][pos.z] = (lightmap[pos.x][pos.y][pos.z] & 0xF0) | v; }
int Chunk::get_sky_light(glm::ivec3 pos) { return (lightmap[pos.x][pos.y][pos.z] >> 4) & 0xF; }
void Chunk::set_sky_light(glm::ivec3 pos, int v) { lightmap[pos.x][pos.y][pos.z] = (lightmap[pos.x][pos.y][pos.z] & 0xF) | (v << 4); }
uint8_t Chunk::get_raw_light(glm::ivec3 pos) { return lightmap[pos.x][pos.y][pos.z]; }

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
    mesh_quad_count = mesh.size() / 36; // 9 floats * 4 vertices
    translucent_quad_count = translucent_mesh.size() / 36;
    send_mesh_data_to_gpu();
}

void Chunk::send_mesh_data_to_gpu() {
#ifdef UNIT_TEST
    return;
#endif
    size_t required_floats = mesh.size() + translucent_mesh.size();
    if(!required_floats) { vbo_capacity = 0; return; }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if(required_floats != vbo_capacity) {
        glBufferData(GL_ARRAY_BUFFER, sizeof(float)*required_floats, NULL, GL_DYNAMIC_DRAW);
        vbo_capacity = required_floats;
    }
    if(!mesh.empty()) glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*mesh.size(), mesh.data());
    if(!translucent_mesh.empty()) glBufferSubData(GL_ARRAY_BUFFER, sizeof(float)*mesh.size(), sizeof(float)*translucent_mesh.size(), translucent_mesh.data());
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
