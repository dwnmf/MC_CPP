#include "item_entity.h"
#include "../world.h"
#include "../renderer/block_type.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace {
constexpr float ITEM_HALF_SIZE = 0.18f; // smaller than a block
constexpr float GRAVITY = -20.0f;
constexpr float BOUNCE_FACTOR = 0.35f;
constexpr float FRICTION = 6.0f;
}

DroppedItem::DroppedItem(World* w, int bid, int c, const glm::vec3& pos, int md)
: block_id(bid), count(c), metadata(md), world(w), position(pos), velocity(0.0f), rotation(0.0f),
  bob_phase(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * glm::two_pi<float>()),
  pickup_cooldown(0.6f), age(0.0f), lifetime(300.0f) {
    // small initial nudge
    float spread = 1.5f;
    velocity.x = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * spread;
    velocity.y = 4.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 1.5f;
    velocity.z = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * spread;
    push_out_of_blocks();
}

Collider DroppedItem::get_collider() const {
    glm::vec3 half(ITEM_HALF_SIZE);
    return Collider(position - half, position + half);
}

bool DroppedItem::intersects_world(const Collider& c) const {
    if (!world) return false;
    int min_x = static_cast<int>(std::floor(c.x1));
    int max_x = static_cast<int>(std::floor(c.x2));
    int min_y = static_cast<int>(std::floor(c.y1));
    int max_y = static_cast<int>(std::floor(c.y2));
    int min_z = static_cast<int>(std::floor(c.z1));
    int max_z = static_cast<int>(std::floor(c.z2));

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                int id = world->get_block_number({x, y, z});
                if (id == 0) continue;
                if (id >= world->block_types.size()) continue;
                auto* bt = world->block_types[id];
                if (!bt) continue;
                for (const auto& col : bt->colliders) {
                    Collider block_col = col + glm::vec3(x, y, z);
                    if (c & block_col) return true;
                }
            }
        }
    }
    return false;
}

void DroppedItem::push_out_of_blocks() {
    Collider c = get_collider();
    if (!intersects_world(c)) return;
    // Try to nudge upwards first, then in small random directions
    const glm::vec3 offsets[] = {
        {0.0f, 0.2f, 0.0f},
        {0.2f, 0.0f, 0.0f},
        {-0.2f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.2f},
        {0.0f, 0.0f, -0.2f}
    };
    for (const auto& off : offsets) {
        position += off;
        if (!intersects_world(get_collider())) return;
    }
}

void DroppedItem::resolve_axis(int axis, float delta) {
    if (delta == 0.0f) return;
    if (axis == 0) position.x += delta;
    else if (axis == 1) position.y += delta;
    else position.z += delta;

    Collider c = get_collider();
    if (intersects_world(c)) {
        if (axis == 0) position.x -= delta;
        else if (axis == 1) position.y -= delta;
        else position.z -= delta;

        float* vel = (axis == 0) ? &velocity.x : (axis == 1) ? &velocity.y : &velocity.z;
        *vel = -*vel * BOUNCE_FACTOR;
        if (std::abs(*vel) < 0.2f) *vel = 0.0f;
    }
}

void DroppedItem::update(float dt) {
    if (removed) return;
    age += dt;
    pickup_cooldown = std::max(0.0f, pickup_cooldown - dt);

    velocity.y += GRAVITY * dt;

    // Apply friction in XZ to make items settle
    float decay = std::exp(-FRICTION * dt);
    velocity.x *= decay;
    velocity.z *= decay;

    resolve_axis(0, velocity.x * dt);
    resolve_axis(1, velocity.y * dt);
    resolve_axis(2, velocity.z * dt);

    rotation = std::fmod(rotation + dt * 1.5f, glm::two_pi<float>());

    if (expired()) removed = true;
}

glm::mat4 DroppedItem::model_matrix() const {
    float bob = std::sin(age * 2.0f + bob_phase) * 0.08f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
    model = glm::scale(model, glm::vec3(0.28f));
    return model;
}
