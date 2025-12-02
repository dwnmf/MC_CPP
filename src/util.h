#pragma once
#include <glm/glm.hpp>
#include <functional>

namespace Util {
    const glm::ivec3 DIRECTIONS[6] = {
        glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0),
        glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0),
        glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)
    };

    // Используем индексы для направлений: 0:East, 1:West, 2:Up, 3:Down, 4:South, 5:North
    const glm::ivec3 EAST(1, 0, 0);
    const glm::ivec3 WEST(-1, 0, 0);
    const glm::ivec3 UP(0, 1, 0);
    const glm::ivec3 DOWN(0, -1, 0);
    const glm::ivec3 SOUTH(0, 0, 1);
    const glm::ivec3 NORTH(0, 0, -1);

    struct IVec3Hash {
        size_t operator()(const glm::ivec3& k) const {
            return std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1) ^ (std::hash<int>()(k.z) << 2);
        }
    };
}
