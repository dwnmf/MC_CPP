#include "subchunk.h"
#include "chunk.h"
#include "../world.h"
#include "../options.h"
#include <algorithm>

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
        return std::vector<float>(4, (float)world->get_light(pos));
    }

    // Determine if model is complex (e.g. Crop, Plant)
    bool complex_model = (bt.vertex_positions.size() != 6);
    // Complex models take light from their own position to avoid black spots
    glm::ivec3 target_pos = complex_model ? pos : npos;

    if (!Options::SMOOTH_LIGHTING) return std::vector<float>(4, (float)world->get_light(target_pos));

    // Skip smooth lighting for complex models
    if (complex_model) return std::vector<float>(4, (float)world->get_light(target_pos));

    std::vector<glm::ivec3> neighbors = get_neighbour_voxels(target_pos, face);
    float l = world->get_light(target_pos);
    return get_smooth_face_light(l, world->get_light(neighbors[0]), world->get_light(neighbors[1]), world->get_light(neighbors[2]),
                                 world->get_light(neighbors[3]), world->get_light(neighbors[4]),
                                 world->get_light(neighbors[5]), world->get_light(neighbors[6]), world->get_light(neighbors[7]));
}

std::vector<float> Subchunk::get_skylight(int block, int face, glm::ivec3 pos, glm::ivec3 npos) {
    BlockType& bt = *world->block_types[block];
    // ИСПРАВЛЕНИЕ: Аналогично для Skylight
    if (!bt.is_cube) {
        return std::vector<float>(4, (float)world->get_skylight(pos));
    }

    // FIX: Аналогично для SkyLight. Slab теперь будет получать мягкое освещение.
    if (!Options::SMOOTH_LIGHTING) return std::vector<float>(4, (float)world->get_skylight(npos));
    std::vector<glm::ivec3> neighbors = get_neighbour_voxels(npos, face);
    if (neighbors.size() < 8) return std::vector<float>(4, (float)world->get_skylight(npos));

    float l = world->get_skylight(npos);
    return get_smooth_face_light(l, world->get_skylight(neighbors[0]), world->get_skylight(neighbors[1]), world->get_skylight(neighbors[2]),
                                 world->get_skylight(neighbors[3]), world->get_skylight(neighbors[4]),
                                 world->get_skylight(neighbors[5]), world->get_skylight(neighbors[6]), world->get_skylight(neighbors[7]));
}

std::vector<float> Subchunk::get_shading(int block, BlockType& bt, int face, glm::ivec3 npos) {
    if (!Options::SMOOTH_LIGHTING) return bt.shading_values[face];

    // Disable AO for complex models (crops) to prevent dark spots
    if (bt.vertex_positions.size() != 6) return std::vector<float>(4, 1.0f);

    std::vector<glm::ivec3> neighbors = get_neighbour_voxels(npos, face);
    if (neighbors.size() < 8) return bt.shading_values[face];

    return get_face_ao(world->is_opaque_block(neighbors[0]), world->is_opaque_block(neighbors[1]), world->is_opaque_block(neighbors[2]),
                       world->is_opaque_block(neighbors[3]), world->is_opaque_block(neighbors[4]),
                       world->is_opaque_block(neighbors[5]), world->is_opaque_block(neighbors[6]), world->is_opaque_block(neighbors[7]));
}

void Subchunk::add_face(int face, glm::ivec3 pos, glm::ivec3 lpos, int block, BlockType& bt, glm::ivec3 npos) {
    auto& target = bt.translucent ? translucent_mesh : mesh;
    std::vector<float> shading = get_shading(block, bt, face, npos);
    std::vector<float> lights = get_light(block, face, pos, npos);
    std::vector<float> skylights = get_skylight(block, face, pos, npos);

    // Проверка на наличие UV в модели (на всякий случай)
    bool has_uv = (face < bt.tex_coords.size());

    for(int i=0; i<4; i++) {
        // Позиция
        target.push_back(bt.vertex_positions[face][i*3+0] + lpos.x);
        target.push_back(bt.vertex_positions[face][i*3+1] + lpos.y);
        target.push_back(bt.vertex_positions[face][i*3+2] + lpos.z);

        // === ИЗМЕНЕНО: Берем UV из модели ===
        if (has_uv) {
            target.push_back(bt.tex_coords[face][i*2+0]); // U
            target.push_back(bt.tex_coords[face][i*2+1]); // V
        } else {
            target.push_back(0.0f); target.push_back(0.0f); // Fallback
        }

        target.push_back((float)bt.tex_indices[face]); // Layer

        target.push_back(shading[i]);
        target.push_back(lights[i]);
        target.push_back(skylights[i]);
    }
}



bool Subchunk::can_render_face(BlockType& bt, int block_number, glm::ivec3 position) {
    // Если соседний блок (position) непрозрачный -> мы не рисуем грань.
    // Если соседний блок прозрачный (воздух) -> мы рисуем грань.
    // Возвращаем true, если нужно рисовать.

    // Получаем номер соседнего блока
    int neighbor_id = world->get_block_number(position);

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
