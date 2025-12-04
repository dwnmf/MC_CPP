#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include "../util.h"
#include "../renderer/block_type.h"

class Chunk;
class World;

const int SUBCHUNK_WIDTH = 16;
const int SUBCHUNK_HEIGHT = 16;
const int SUBCHUNK_LENGTH = 16;

class Subchunk {
public:
    Chunk* parent;
    World* world;
    glm::ivec3 subchunk_position;
    glm::ivec3 local_position;
    glm::vec3 position;

    std::vector<uint32_t> mesh;
    std::vector<uint32_t> translucent_mesh;

    Subchunk(Chunk* p, glm::ivec3 pos);
    void update_mesh();

private:
    float smooth(float a, float b, float c, float d);
    float ao_val(bool s1, bool s2, bool c);
    std::array<float, 4> get_face_ao(bool s1, bool s2, bool s3, bool s4, bool s5, bool s6, bool s7, bool s8);
    std::array<float, 4> get_smooth_face_light(float light, float l1, float l2, float l3, float l4, float l5, float l6, float l7, float l8);
    std::array<glm::ivec3, 8> get_neighbour_voxels(glm::ivec3 npos, int face);
    std::array<float, 4> get_light(int block, int face, glm::ivec3 pos, glm::ivec3 npos);
    std::array<float, 4> get_skylight(int block, int face, glm::ivec3 pos, glm::ivec3 npos);
    std::array<float, 4> get_shading(int block, BlockType& bt, int face, glm::ivec3 npos);
    void add_face(int face, glm::ivec3 pos, glm::ivec3 lpos, int block, BlockType& bt, glm::ivec3 npos);
    bool can_render_face(BlockType& bt, int block_number, glm::ivec3 position);
};
