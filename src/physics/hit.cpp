#include "hit.h"
#include "../world.h"
#include <cmath>

HitRay::HitRay(World* w, glm::vec2 rotation, glm::vec3 start_pos) : world(w) {
    vector = glm::vec3(
        cos(rotation.x) * cos(rotation.y),
                       sin(rotation.y),
                       sin(rotation.x) * cos(rotation.y)
    );
    position = start_pos;
    block = glm::round(position);
    distance = 0;
}

bool HitRay::check(std::function<void(glm::ivec3, glm::ivec3)> callback, float dist, glm::ivec3 current, glm::ivec3 next) {
    if (world->get_block_number(next)) {
        callback(current, next);
        return true;
    } else {
        position += vector * dist;
        block = next;
        distance += dist;
        return false;
    }
}

bool HitRay::step(std::function<void(glm::ivec3, glm::ivec3)> callback) {
    float bx = block.x; float by = block.y; float bz = block.z;
    glm::vec3 local = position - glm::vec3(block);

    glm::vec3 sign;
    glm::vec3 abs_vec = vector;

    for (int i = 0; i < 3; i++) {
        sign[i] = (vector[i] >= 0) ? 1.0f : -1.0f;
        abs_vec[i] *= sign[i];
        local[i] *= sign[i];
    }

    if (abs_vec.x) {
        float x = 0.5;
        float y = (0.5 - local.x) / abs_vec.x * abs_vec.y + local.y;
        float z = (0.5 - local.x) / abs_vec.x * abs_vec.z + local.z;
        if (y >= -0.5 && y <= 0.5 && z >= -0.5 && z <= 0.5) {
            float dist = sqrt(pow(x - local.x, 2) + pow(y - local.y, 2) + pow(z - local.z, 2));
            return check(callback, dist, block, block + glm::ivec3(sign.x, 0, 0));
        }
    }
    if (abs_vec.y) {
        float x = (0.5 - local.y) / abs_vec.y * abs_vec.x + local.x;
        float y = 0.5;
        float z = (0.5 - local.y) / abs_vec.y * abs_vec.z + local.z;
        if (x >= -0.5 && x <= 0.5 && z >= -0.5 && z <= 0.5) {
            float dist = sqrt(pow(x - local.x, 2) + pow(y - local.y, 2) + pow(z - local.z, 2));
            return check(callback, dist, block, block + glm::ivec3(0, sign.y, 0));
        }
    }
    if (abs_vec.z) {
        float x = (0.5 - local.z) / abs_vec.z * abs_vec.x + local.x;
        float y = (0.5 - local.z) / abs_vec.z * abs_vec.y + local.y;
        float z = 0.5;
        if (x >= -0.5 && x <= 0.5 && y >= -0.5 && y <= 0.5) {
            float dist = sqrt(pow(x - local.x, 2) + pow(y - local.y, 2) + pow(z - local.z, 2));
            return check(callback, dist, block, block + glm::ivec3(0, 0, sign.z));
        }
    }
    return false;
}
