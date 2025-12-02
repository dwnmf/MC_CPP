#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "../physics/collider.h"

struct ModelData {
    bool transparent = false;
    bool is_cube = true;
    bool glass = false;
    bool translucent = false;

    std::vector<std::pair<glm::vec3, glm::vec3>> colliders;
    std::vector<std::vector<float>> vertex_positions;
    std::vector<std::vector<float>> tex_coords;
    std::vector<std::vector<float>> shading_values;
    size_t tex_coords_len = 6;
};
