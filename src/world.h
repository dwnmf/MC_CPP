#pragma once
#include <vector>
#include <deque>
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "chunk/chunk.h"
#include "entity/player.h"
#include "renderer/shader.h"
#include "renderer/texture_manager.h"
#include "options.h"
#include "save.h"
#include "util.h"
#include "physics/collider.h"

class World {
public:
    Shader* shader;
    Player* player;
    TextureManager* texture_manager;
    std::vector<BlockType*> block_types;
    std::unordered_map<glm::ivec3, Chunk*, Util::IVec3Hash> chunks;
    std::vector<Chunk*> visible_chunks;

    std::deque<std::pair<glm::ivec3, int>> light_increase_queue;
    std::deque<std::pair<glm::ivec3, int>> light_decrease_queue;
    std::deque<std::pair<glm::ivec3, int>> skylight_increase_queue;
    std::deque<std::pair<glm::ivec3, int>> skylight_decrease_queue;
    std::deque<Chunk*> chunk_building_queue;

    std::vector<int> light_blocks = {10, 11, 50, 51, 62, 75};

    float daylight = 1800;
    int incrementer = 0;
    long time = 0;
    int chunk_update_counter = 0;
    int pending_chunk_update_count = 0;
    GLuint ibo;
    int shader_daylight_loc;
    Save* save_system;

    World(Shader* s, TextureManager* tm, Player* p);
    ~World();

    void tick(float dt);
    void draw();
    void draw_translucent();

    void set_block(glm::ivec3 pos, int number);
    bool try_set_block(glm::ivec3 pos, int number, const Collider& player_collider);

    int get_block_number(glm::ivec3 pos);
    int get_light(glm::ivec3 pos);
    int get_skylight(glm::ivec3 pos);
    void increase_light(glm::ivec3 pos, int newlight, bool update=true);
    void decrease_light(glm::ivec3 pos);
    void decrease_skylight(glm::ivec3 pos);

    void init_skylight(Chunk* chunk);

    bool is_opaque_block(glm::ivec3 pos);
    bool get_transparency(glm::ivec3 pos);

    glm::ivec3 get_chunk_pos(glm::vec3 pos);
    glm::ivec3 get_local_pos(glm::vec3 pos);

    void propagate_increase(bool update);
    void propagate_decrease(bool update);
    void propagate_skylight_increase(bool update);
    void propagate_skylight_decrease(bool update);

    void prepare_rendering();
    void speed_daytime();
};
