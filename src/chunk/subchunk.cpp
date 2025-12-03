#include "subchunk.h"
#include "chunk.h"
#include "../world.h"
#include "../options.h"
#include <algorithm>
#include <cmath>

namespace {
inline int16_t pack_pos_component(float v) {
    int val = static_cast<int>(std::round(v * 16.0f));
    val = std::clamp(val, -32768, 32767);
    return static_cast<int16_t>(val);
}

inline uint8_t pack_uv(float v) {
    int val = static_cast<int>(std::round(v * 255.0f));
    return static_cast<uint8_t>(std::clamp(val, 0, 255));
}

inline uint8_t pack_shading(float v) {
    int val = static_cast<int>(std::round(v * 255.0f));
    return static_cast<uint8_t>(std::clamp(val, 0, 255));
}

inline uint32_t pack_pos_xy(int16_t x, int16_t y) {
    return static_cast<uint16_t>(x) | (static_cast<uint32_t>(static_cast<uint16_t>(y)) << 16);
}

inline uint32_t pack_pos_z_uv(int16_t z, uint8_t u, uint8_t v) {
    return static_cast<uint16_t>(z) | (static_cast<uint32_t>(u) << 16) | (static_cast<uint32_t>(v) << 24);
}

inline uint32_t pack_attr(uint8_t layer, uint8_t shading, uint8_t blocklight, uint8_t skylight) {
    return static_cast<uint32_t>(layer) |
           (static_cast<uint32_t>(shading) << 8) |
           (static_cast<uint32_t>(blocklight & 0xF) << 16) |
           (static_cast<uint32_t>(skylight & 0xF) << 20);
}
}

Subchunk::Subchunk(Chunk* p, glm::ivec3 pos) : parent(p), world(p->world), subchunk_position(pos) {
    local_position = pos * glm::ivec3(SUBCHUNK_WIDTH, SUBCHUNK_HEIGHT, SUBCHUNK_LENGTH);
    position = p->position + glm::vec3(local_position);
}

float Subchunk::smooth(float a, float b, float c, float d) {
    // Если хоть один компонент равен 0 (или отсутствует), пытаемся сгладить
    if (a == 0 || b == 0 || c == 0 || d == 0) {
        // Логика из Python: light = (a, *(i for i in (b, c, d) if i))
        // Ищем минимум среди 'a' и ненулевых соседей.
        // Если 'a' (центр) равен 0, то минимум всегда будет 0.

        float min_val = a;
        if (a > 0) {
            if (b > 0) min_val = std::min(min_val, b);
            if (c > 0) min_val = std::min(min_val, c);
            if (d > 0) min_val = std::min(min_val, d);
        } else {
            min_val = 0.0f;
        }

        a = std::max(a, min_val);
        b = std::max(b, min_val);
        c = std::max(c, min_val);
        d = std::max(d, min_val);
    }
    return (a + b + c + d) / 4.0f;
}


float Subchunk::ao_val(bool s1, bool s2, bool c) {
    if (s1 && s2) return 0.25f;
    return 1.0f - (s1 + s2 + c) / 4.0f;
}

std::vector<float> Subchunk::get_face_ao(bool s1, bool s2, bool s3, bool s4, bool s5, bool s6, bool s7, bool s8) {
    return {ao_val(s2, s4, s1), ao_val(s4, s7, s6), ao_val(s5, s7, s8), ao_val(s2, s5, s3)};
}

std::vector<float> Subchunk::get_smooth_face_light(float light, float l1, float l2, float l3, float l4, float l5, float l6, float l7, float l8) {
    return {smooth(light, l2, l4, l1), smooth(light, l4, l7, l6), smooth(light, l5, l7, l8), smooth(light, l2, l5, l3)};
}

std::vector<glm::ivec3> Subchunk::get_neighbour_voxels(glm::ivec3 npos, int face) {
    using namespace Util;
    std::vector<glm::ivec3> n;
    if(face==0) n={npos+UP+SOUTH, npos+UP, npos+UP+NORTH, npos+SOUTH, npos+NORTH, npos+DOWN+SOUTH, npos+DOWN, npos+DOWN+NORTH};
    else if(face==1) n={npos+UP+NORTH, npos+UP, npos+UP+SOUTH, npos+NORTH, npos+SOUTH, npos+DOWN+NORTH, npos+DOWN, npos+DOWN+SOUTH};
    else if(face==2) n={npos+SOUTH+EAST, npos+SOUTH, npos+SOUTH+WEST, npos+EAST, npos+WEST, npos+NORTH+EAST, npos+NORTH, npos+NORTH+WEST};
    else if(face==3) n={npos+SOUTH+WEST, npos+SOUTH, npos+SOUTH+EAST, npos+WEST, npos+EAST, npos+NORTH+WEST, npos+NORTH, npos+NORTH+EAST};
    else if(face==4) n={npos+UP+WEST, npos+UP, npos+UP+EAST, npos+WEST, npos+EAST, npos+DOWN+WEST, npos+DOWN, npos+DOWN+EAST};
    else if(face==5) n={npos+UP+EAST, npos+UP, npos+UP+WEST, npos+EAST, npos+WEST, npos+DOWN+EAST, npos+DOWN, npos+DOWN+WEST};
    return n;
}

std::vector<float> Subchunk::get_light(int block, int face, glm::ivec3 pos, glm::ivec3 npos) {
    BlockType& bt = *world->block_types[block];
    if (!bt.is_cube) {
        // ДЛЯ НЕПОЛНЫХ БЛОКОВ БЕРЕМ СВЕТ ИЗ ТЕКУЩЕЙ ПОЗИЦИИ (pos), А НЕ ИЗ СОСЕДА (npos)
        return std::vector<float>(4, (float)parent->get_light_cached(pos));
    }

    // Determine if model is complex (e.g. Crop, Plant)
    bool complex_model = (bt.vertex_positions.size() != 6);
    // Complex models take light from their own position to avoid black spots
    glm::ivec3 target_pos = complex_model ? pos : npos;

    if (!Options::SMOOTH_LIGHTING) return std::vector<float>(4, (float)parent->get_light_cached(target_pos));

    // Skip smooth lighting for complex models
    if (complex_model) return std::vector<float>(4, (float)parent->get_light_cached(target_pos));

    std::vector<glm::ivec3> neighbors = get_neighbour_voxels(target_pos, face);
    float l = parent->get_light_cached(target_pos);
    return get_smooth_face_light(l, parent->get_light_cached(neighbors[0]), parent->get_light_cached(neighbors[1]), parent->get_light_cached(neighbors[2]),
                                 parent->get_light_cached(neighbors[3]), parent->get_light_cached(neighbors[4]),
                                 parent->get_light_cached(neighbors[5]), parent->get_light_cached(neighbors[6]), parent->get_light_cached(neighbors[7]));
}

std::vector<float> Subchunk::get_skylight(int block, int face, glm::ivec3 pos, glm::ivec3 npos) {
    BlockType& bt = *world->block_types[block];
    // ИСПРАВЛЕНИЕ: Аналогично для Skylight
    if (!bt.is_cube) {
        return std::vector<float>(4, (float)parent->get_skylight_cached(pos));
    }

    // FIX: Аналогично для SkyLight. Slab теперь будет получать мягкое освещение.
    if (!Options::SMOOTH_LIGHTING) return std::vector<float>(4, (float)parent->get_skylight_cached(npos));
    std::vector<glm::ivec3> neighbors = get_neighbour_voxels(npos, face);
    if (neighbors.size() < 8) return std::vector<float>(4, (float)parent->get_skylight_cached(npos));

    float l = parent->get_skylight_cached(npos);
    return get_smooth_face_light(l, parent->get_skylight_cached(neighbors[0]), parent->get_skylight_cached(neighbors[1]), parent->get_skylight_cached(neighbors[2]),
                                 parent->get_skylight_cached(neighbors[3]), parent->get_skylight_cached(neighbors[4]),
                                 parent->get_skylight_cached(neighbors[5]), parent->get_skylight_cached(neighbors[6]), parent->get_skylight_cached(neighbors[7]));
}

std::vector<float> Subchunk::get_shading(int block, BlockType& bt, int face, glm::ivec3 npos) {
    if (!Options::SMOOTH_LIGHTING) return bt.shading_values[face];

    // Disable AO for complex models (crops) to prevent dark spots
    if (bt.vertex_positions.size() != 6) return std::vector<float>(4, 1.0f);

    std::vector<glm::ivec3> neighbors = get_neighbour_voxels(npos, face);
    if (neighbors.size() < 8) return bt.shading_values[face];

    return get_face_ao(parent->is_opaque_cached(neighbors[0]), parent->is_opaque_cached(neighbors[1]), parent->is_opaque_cached(neighbors[2]),
                       parent->is_opaque_cached(neighbors[3]), parent->is_opaque_cached(neighbors[4]),
                       parent->is_opaque_cached(neighbors[5]), parent->is_opaque_cached(neighbors[6]), parent->is_opaque_cached(neighbors[7]));
}

void Subchunk::add_face(int face, glm::ivec3 pos, glm::ivec3 lpos, int block, BlockType& bt, glm::ivec3 npos) {
    auto& target = bt.translucent ? translucent_mesh : mesh;
    std::vector<float> shading = get_shading(block, bt, face, npos);
    std::vector<float> lights = get_light(block, face, pos, npos);
    std::vector<float> skylights = get_skylight(block, face, pos, npos);

    bool has_uv = (face < bt.tex_coords.size());

    for(int i=0; i<4; i++) {
        float vx = bt.vertex_positions[face][i*3+0] + lpos.x;
        float vy = bt.vertex_positions[face][i*3+1] + lpos.y;
        float vz = bt.vertex_positions[face][i*3+2] + lpos.z;

        uint8_t u = 0, v = 0;
        if (has_uv) {
            u = pack_uv(bt.tex_coords[face][i*2+0]);
            v = pack_uv(bt.tex_coords[face][i*2+1]);
        }

        uint8_t layer = static_cast<uint8_t>(bt.tex_indices[face]);
        uint8_t shade = pack_shading(shading[i]);
        uint8_t bl = static_cast<uint8_t>(std::clamp<int>(static_cast<int>(lights[i]), 0, 15));
        uint8_t sl = static_cast<uint8_t>(std::clamp<int>(static_cast<int>(skylights[i]), 0, 15));

        int16_t px = pack_pos_component(vx);
        int16_t py = pack_pos_component(vy);
        int16_t pz = pack_pos_component(vz);

        target.push_back(pack_pos_xy(px, py));
        target.push_back(pack_pos_z_uv(pz, u, v));
        target.push_back(pack_attr(layer, shade, bl, sl));
    }
}



bool Subchunk::can_render_face(BlockType& bt, int block_number, glm::ivec3 position) {
    // Если соседний блок (position) непрозрачный -> мы не рисуем грань.
    // Если соседний блок прозрачный (воздух) -> мы рисуем грань.
    // Возвращаем true, если нужно рисовать.

    // Получаем номер соседнего блока
    int neighbor_id = parent->get_block_number_cached(position);

    // Если сосед - воздух (0), рисуем
    if (neighbor_id == 0) return true;

    // Если блок "стекло" и сосед тоже "стекло", не рисуем внутренние грани (опционально)
    if (bt.glass && neighbor_id == block_number) return false;

    // Если сосед прозрачный (листва, стекло), а мы твердый блок, рисуем
    BlockType* neighbor_type = world->block_types[neighbor_id];
    if (neighbor_type && neighbor_type->transparent) return true;

    // Иначе (сосед твердый) - не рисуем
    return false;
}

void Subchunk::update_mesh() {
    mesh.clear(); translucent_mesh.clear();
    for (int x=0; x<SUBCHUNK_WIDTH; x++)
        for (int y=0; y<SUBCHUNK_HEIGHT; y++)
            for (int z=0; z<SUBCHUNK_LENGTH; z++) {
                int lx = local_position.x + x; int ly = local_position.y + y; int lz = local_position.z + z;
                int bn = parent->blocks[lx][ly][lz];
                if (!bn) continue;
                BlockType& bt = *world->block_types[bn];
                glm::ivec3 pos = glm::ivec3(position) + glm::ivec3(x, y, z);
                glm::ivec3 lpos(lx, ly, lz);

                if (bt.is_cube) {
                    for(int f=0; f<6; f++) {
                        glm::ivec3 npos = pos + Util::DIRECTIONS[f];
                        if (can_render_face(bt, bn, npos)) add_face(f, pos, lpos, bn, bt, npos);
                    }
                } else {
                    for(int f=0; f<bt.vertex_positions.size(); f++) add_face(f, pos, lpos, bn, bt, pos);
                }
            }
}
