#pragma once
#include <glm/glm.hpp>
#include "../physics/collider.h"

class World;

class Entity {
public:
    World* world;
    glm::vec3 position;
    glm::vec3 old_position; // Для интерполяции
    glm::vec2 rotation;
    glm::vec3 velocity;
    glm::vec3 accel;
    Collider collider;

    float width = 0.6f;
    float height = 1.8f;
    bool grounded = false;
    bool flying = false;

    // Новая переменная для плавного шага
    float step_offset = 0.0f;

    Entity(World* w);
    virtual void update(float dt);
    void update_collider();
    void jump();
};
