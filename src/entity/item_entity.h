#pragma once
#include <glm/glm.hpp>
#include "../physics/collider.h"

class World;

class DroppedItem {
public:
    DroppedItem(World* world, int block_id, int count, const glm::vec3& pos, int metadata = 0);

    void update(float dt);
    Collider get_collider() const;
    glm::mat4 model_matrix() const;

    bool can_pickup() const { return pickup_cooldown <= 0.0f; }
    bool should_remove() const { return removed; }
    bool expired() const { return age >= lifetime; }
    glm::vec3 get_position() const { return position; }

    void mark_removed() { removed = true; }

    int block_id;
    int count;
    int metadata;

private:
    World* world;
    glm::vec3 position;
    glm::vec3 velocity;
    float rotation;
    float bob_phase;
    float pickup_cooldown;
    float age;
    float lifetime;
    bool removed = false;

    bool intersects_world(const Collider& c) const;
    void push_out_of_blocks();
    void resolve_axis(int axis, float delta);
};
