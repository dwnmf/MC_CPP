#pragma once
#include <string>
#include <vector>
#include <map>
#include "../models/model_data.h"
#include "texture_manager.h"
#include "../physics/collider.h"

class BlockType {
public:
    std::string name;
    ModelData model;
    std::map<std::string, std::string> block_face_textures;

    bool transparent;
    bool is_cube;
    bool glass;
    bool translucent;

    std::vector<Collider> colliders;
    std::vector<std::vector<float>> vertex_positions;
    std::vector<std::vector<float>> tex_coords; // <-- ДОБАВЛЕНО
    std::vector<int> tex_indices;
    std::vector<std::vector<float>> shading_values;

    BlockType() {}
    BlockType(TextureManager* tm, std::string name, std::map<std::string, std::string> face_tex, ModelData md);
};
