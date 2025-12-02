#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>
#include <tuple>

class Collider {
public:
    float x1, y1, z1;
    float x2, y2, z2;

    Collider(glm::vec3 pos1 = glm::vec3(0), glm::vec3 pos2 = glm::vec3(0)) {
        x1 = pos1.x; y1 = pos1.y; z1 = pos1.z;
        x2 = pos2.x; y2 = pos2.y; z2 = pos2.z;
    }

    Collider operator+(const glm::vec3& pos) const {
        return Collider(glm::vec3(x1 + pos.x, y1 + pos.y, z1 + pos.z),
                        glm::vec3(x2 + pos.x, y2 + pos.y, z2 + pos.z));
    }

    bool operator&(const Collider& other) const {
        float x = std::min(x2, other.x2) - std::max(x1, other.x1);
        float y = std::min(y2, other.y2) - std::max(y1, other.y1);
        float z = std::min(z2, other.z2) - std::max(z1, other.z1);
        return x > 0 && y > 0 && z > 0;
    }

    std::pair<float, glm::vec3> collide(const Collider& static_col, const glm::vec3& velocity) {
        float vx = velocity.x;
        float vy = velocity.y;
        float vz = velocity.z;

        auto time_func = [](float x, float y) -> float {
            if (y == 0) return (x > 0) ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
            return x / y;
        };

        float x_entry = time_func(vx > 0 ? static_col.x1 - x2 : static_col.x2 - x1, vx);
        float x_exit = time_func(vx > 0 ? static_col.x2 - x1 : static_col.x1 - x2, vx);
        float y_entry = time_func(vy > 0 ? static_col.y1 - y2 : static_col.y2 - y1, vy);
        float y_exit = time_func(vy > 0 ? static_col.y2 - y1 : static_col.y1 - y2, vy);
        float z_entry = time_func(vz > 0 ? static_col.z1 - z2 : static_col.z2 - z1, vz);
        float z_exit = time_func(vz > 0 ? static_col.z2 - z1 : static_col.z1 - z2, vz);

        if (x_entry < 0 && y_entry < 0 && z_entry < 0) return { 1.0f, glm::vec3(0) };
        if (x_entry > 1 || y_entry > 1 || z_entry > 1) return { 1.0f, glm::vec3(0) };

        float entry = std::max({x_entry, y_entry, z_entry});
        float exit_ = std::min({x_exit, y_exit, z_exit});

        if (entry > exit_) return { 1.0f, glm::vec3(0) };

        glm::vec3 normal(0);
        if (entry == x_entry) normal.x = (vx > 0) ? -1 : 1;
        else if (entry == y_entry) normal.y = (vy > 0) ? -1 : 1;
        else normal.z = (vz > 0) ? -1 : 1;

        return { entry, normal };
    }
};
