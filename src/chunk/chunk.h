#pragma once
#include <vector>
#include <deque>
#include <map>
#include <tuple>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "subchunk.h"

class World;

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 128;
const int CHUNK_LENGTH = 16;

class Chunk {
public:
    World* world;
    glm::ivec3 chunk_position;
    glm::vec3 position;
    bool modified = false;

    uint8_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_LENGTH];
    uint8_t lightmap[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_LENGTH];

    std::map<std::tuple<int, int, int>, Subchunk*> subchunks;
    std::deque<Subchunk*> chunk_update_queue;

    std::vector<float> mesh;
    std::vector<float> translucent_mesh;
    int mesh_quad_count = 0;
    int translucent_quad_count = 0;

    GLuint vao = 0, vbo = 0;
    size_t vbo_capacity = 0;
    int shader_chunk_offset_loc = -1;

    Chunk(World* w, glm::ivec3 pos);
    ~Chunk();

    int get_block_light(glm::ivec3 pos);
    void set_block_light(glm::ivec3 pos, int value);
    int get_sky_light(glm::ivec3 pos);
    void set_sky_light(glm::ivec3 pos, int value);
    uint8_t get_raw_light(glm::ivec3 pos);

    void update_subchunk_meshes();
    void update_at_position(glm::ivec3 pos);
    void process_chunk_updates();
    void update_mesh();
    void send_mesh_data_to_gpu();
    void draw(GLenum mode);
    void draw_translucent(GLenum mode);
};
