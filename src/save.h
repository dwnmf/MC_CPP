#pragma once
#include <string>
#include <deque>
#include <glm/glm.hpp>

class World;
class Chunk;

class Save {
public:
    World* world;
    std::string path;

    Save(World* w);
    void save();

    // Loads a minimal chunk set immediately and queues the rest for streaming.
    void load(int initial_radius = 2);
    void update_streaming(glm::vec3 player_pos);
    void stream_next(int max_chunks = 1);
    bool has_pending_chunks() const { return !pending_chunks.empty(); }

private:
    bool load_chunk(const glm::ivec3& pos, bool eager_build);
    bool save_chunk(Chunk* chunk);
    std::deque<glm::ivec3> pending_chunks;
    glm::ivec3 last_center_chunk = glm::ivec3(999999);
};
