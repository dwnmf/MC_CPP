#pragma once
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
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

    std::unordered_set<int> light_blocks = {10, 11, 50, 51, 62, 75};

    float daylight = 1800;
    int incrementer = 0;
    long time = 0;
    int chunk_update_counter = 0;
    int pending_chunk_update_count = 0;
    GLuint ibo = 0;
    int shader_daylight_loc = -1;

    // Shadow mapping resources
    Shader* shadow_shader = nullptr;
    GLuint shadow_fbo = 0;
    GLuint shadow_map = 0;
    int shadow_map_resolution = 0;
    int shadow_cascade_count = 0;
    bool shadows_enabled = false;

    std::vector<glm::mat4> shadow_matrices;
    std::vector<float> shadow_splits;

    // Cached uniform locations in the main world shader
    int shader_shadow_map_loc = -1;
    int shader_light_space_mats_loc = -1;
    int shader_cascade_splits_loc = -1;
    int shader_cascade_count_loc = -1;
    int shader_shadow_texel_size_loc = -1;
    int shader_shadow_min_bias_loc = -1;
    int shader_shadow_slope_bias_loc = -1;
    int shader_shadow_pcf_radius_loc = -1;

    Save* save_system = nullptr;

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

    void propagate_increase(bool update, int max_steps = -1);
    void propagate_decrease(bool update, int max_steps = -1);
    void propagate_skylight_increase(bool update, int max_steps = -1);
    void propagate_skylight_decrease(bool update, int max_steps = -1);
    void stitch_block_light(class Chunk* c);
    void stitch_sky_light(class Chunk* c);

    void prepare_rendering();
    void render_shadows();

    bool init_shadow_resources();
    void update_shadow_cascades();

    void speed_daytime();
    glm::vec3 get_light_direction() const;
    float get_daylight_factor() const;
};
