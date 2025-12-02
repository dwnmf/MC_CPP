#pragma once
#include <glm/glm.hpp>
#include <functional>

class World;

class HitRay {
public:
    World* world;
    glm::vec3 vector;
    glm::vec3 position;
    glm::ivec3 block;
    float distance;

    HitRay(World* w, glm::vec2 rotation, glm::vec3 start_pos);
    bool check(std::function<void(glm::ivec3, glm::ivec3)> callback, float dist, glm::ivec3 current, glm::ivec3 next);
    bool step(std::function<void(glm::ivec3, glm::ivec3)> callback);
};
