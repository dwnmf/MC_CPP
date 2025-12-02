#include "all_models.h"

namespace Models {
    std::vector<std::vector<float>> default_shading(size_t faces) {
        std::vector<std::vector<float>> s;
        if(faces == 6) {
            s = {{0.6,0.6,0.6,0.6}, {0.6,0.6,0.6,0.6}, {1.0,1.0,1.0,1.0}, {0.4,0.4,0.4,0.4}, {0.8,0.8,0.8,0.8}, {0.8,0.8,0.8,0.8}};
        } else {
            for(size_t i=0; i<faces; i++) s.push_back({1.0,1.0,1.0,1.0});
        }
        return s;
    }

    const std::vector<float> UV_FULL = {0.0, 1.0,  0.0, 0.0,  1.0, 0.0,  1.0, 1.0};

    namespace Cube {
        ModelData get_model() {
            ModelData m;
            m.colliders.push_back({glm::vec3(-0.5), glm::vec3(0.5)});
            m.vertex_positions = {
                { 0.5, 0.5, 0.5, 0.5,-0.5, 0.5, 0.5,-0.5,-0.5, 0.5, 0.5,-0.5},
                {-0.5, 0.5,-0.5,-0.5,-0.5,-0.5,-0.5,-0.5, 0.5,-0.5, 0.5, 0.5},
                { 0.5, 0.5, 0.5, 0.5, 0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5, 0.5},
                {-0.5,-0.5, 0.5,-0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,-0.5, 0.5},
                {-0.5, 0.5, 0.5,-0.5,-0.5, 0.5, 0.5,-0.5, 0.5, 0.5, 0.5, 0.5},
                { 0.5, 0.5,-0.5, 0.5,-0.5,-0.5,-0.5,-0.5,-0.5,-0.5, 0.5,-0.5}
            };
            for(int i=0; i<6; ++i) m.tex_coords.push_back(UV_FULL);
            m.shading_values = default_shading(6);
            return m;
        }
    }

    namespace Plant {
        ModelData get_model() {
            ModelData m;
            m.transparent = true; m.is_cube = false;
            m.vertex_positions = {
                {-0.3536, 0.5, 0.3536, -0.3536, -0.5, 0.3536, 0.3536, -0.5, -0.3536, 0.3536, 0.5, -0.3536},
                {-0.3536, 0.5, -0.3536, -0.3536, -0.5, -0.3536, 0.3536, -0.5, 0.3536, 0.3536, 0.5, 0.3536},
                {0.3536, 0.5, -0.3536, 0.3536, -0.5, -0.3536, -0.3536, -0.5, 0.3536, -0.3536, 0.5, 0.3536},
                {0.3536, 0.5, 0.3536, 0.3536, -0.5, 0.3536, -0.3536, -0.5, -0.3536, -0.3536, 0.5, -0.3536}
            };
            for(int i=0; i<4; ++i) m.tex_coords.push_back(UV_FULL);
            m.shading_values = default_shading(4);
            m.tex_coords_len = 4;
            return m;
        }
    }

    // ИСПРАВЛЕНО: Добавлен флаг transparent
    namespace Leaves {
        ModelData get_model() {
            ModelData m = Cube::get_model();
            m.transparent = true;
            return m;
        }
    }

    // ИСПРАВЛЕНО: Флаги transparent и glass
    namespace Glass {
        ModelData get_model() {
            ModelData m = Cube::get_model();
            m.transparent = true;
            m.glass = true;
            return m;
        }
    }

    // ИСПРАВЛЕНО: Флаг transparent (иначе лестница рисуется с черным фоном или багами)
    namespace Ladder {
        ModelData get_model() {
            ModelData m = Cube::get_model();
            m.transparent = true;
            m.is_cube = false;
            return m;
        }
    }

    namespace Liquid {
        ModelData get_model() {
            ModelData m;
            m.transparent = true; m.glass = true; m.translucent = true;
            m.vertex_positions = {
                { 0.5, 0.375, 0.5, 0.5,-0.625, 0.5, 0.5,-0.625,-0.5, 0.5, 0.375,-0.5},
                {-0.5, 0.375,-0.5,-0.5,-0.625,-0.5,-0.5,-0.625, 0.5,-0.5, 0.375, 0.5},
                { 0.5, 0.375, 0.5, 0.5, 0.375,-0.5,-0.5, 0.375,-0.5,-0.5, 0.375, 0.5},
                {-0.5,-0.625, 0.5,-0.5,-0.625,-0.5, 0.5,-0.625,-0.5, 0.5,-0.625, 0.5},
                {-0.5, 0.375, 0.5,-0.5,-0.625, 0.5, 0.5,-0.625, 0.5, 0.5, 0.375, 0.5},
                { 0.5, 0.375,-0.5, 0.5,-0.625,-0.5,-0.5,-0.625,-0.5,-0.5, 0.375,-0.5}
            };
            for(int i=0; i<6; ++i) m.tex_coords.push_back(UV_FULL);
            m.shading_values = default_shading(6);
            return m;
        }
    }

    namespace Slab {
        ModelData get_model() {
            ModelData m;
            m.transparent = true; m.is_cube = false;
            m.colliders.push_back({glm::vec3(-0.5,-0.5,-0.5), glm::vec3(0.5,0.0,0.5)});
            m.vertex_positions = {
                { 0.5, 0.0, 0.5, 0.5,-0.5, 0.5, 0.5,-0.5,-0.5, 0.5, 0.0,-0.5},
                {-0.5, 0.0,-0.5,-0.5,-0.5,-0.5,-0.5,-0.5, 0.5,-0.5, 0.0, 0.5},
                { 0.5, 0.0, 0.5, 0.5, 0.0,-0.5,-0.5, 0.0,-0.5,-0.5, 0.0, 0.5},
                {-0.5,-0.5, 0.5,-0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,-0.5, 0.5},
                {-0.5, 0.0, 0.5,-0.5,-0.5, 0.5, 0.5,-0.5, 0.5, 0.5, 0.0, 0.5},
                { 0.5, 0.0,-0.5, 0.5,-0.5,-0.5,-0.5,-0.5,-0.5,-0.5, 0.0,-0.5}
            };

            std::vector<float> UV_HALF = {0.0, 0.5,  0.0, 0.0,  1.0, 0.0,  1.0, 0.5};
            m.tex_coords.push_back(UV_HALF); m.tex_coords.push_back(UV_HALF);
            m.tex_coords.push_back(UV_FULL); m.tex_coords.push_back(UV_FULL);
            m.tex_coords.push_back(UV_HALF); m.tex_coords.push_back(UV_HALF);

            m.shading_values = default_shading(6);
            return m;
        }
    }

    namespace Torch {
        ModelData get_model() {
            ModelData m; m.transparent = true; m.is_cube = false;
            m.vertex_positions = {
                { 0.0625, 0.5, 0.5, 0.0625,-0.5, 0.5, 0.0625,-0.5,-0.5, 0.0625, 0.5,-0.5},
                {-0.0625, 0.5,-0.5,-0.0625,-0.5,-0.5,-0.0625,-0.5, 0.5,-0.0625, 0.5, 0.5},
                { 0.5, 0.125, 0.5, 0.5, 0.125,-0.5,-0.5, 0.125,-0.5,-0.5, 0.125, 0.5},
                {-0.5,-0.5, 0.5,-0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,-0.5, 0.5},
                {-0.5, 0.5, 0.0625,-0.5,-0.5, 0.0625, 0.5,-0.5, 0.0625, 0.5, 0.5, 0.0625},
                { 0.5, 0.5,-0.0625, 0.5,-0.5,-0.0625,-0.5,-0.5,-0.0625,-0.5, 0.5,-0.0625}
            };
            for(int i=0; i<6; ++i) m.tex_coords.push_back(UV_FULL);
            m.shading_values = default_shading(6);
            return m;
        }
    }

    namespace Door {
        ModelData get_model() {
            ModelData m = Cube::get_model();
            m.transparent = true; m.is_cube = false;
            m.vertex_positions = {
                { 0.5, 0.5, 0.1, 0.5,-0.5, 0.1, 0.5,-0.5,-0.1, 0.5, 0.5,-0.1},
                {-0.5, 0.5,-0.1,-0.5,-0.5,-0.1,-0.5,-0.5, 0.1,-0.5, 0.5, 0.1},
                { 0.5, 0.5, 0.1, 0.5, 0.5,-0.1,-0.5, 0.5,-0.1,-0.5, 0.5, 0.1},
                {-0.5,-0.5, 0.1,-0.5,-0.5,-0.1, 0.5,-0.5,-0.1, 0.5,-0.5, 0.1},
                {-0.5, 0.5, 0.1,-0.5,-0.5, 0.1, 0.5,-0.5, 0.1, 0.5, 0.5, 0.1},
                { 0.5, 0.5,-0.1, 0.5,-0.5,-0.1,-0.5,-0.5,-0.1,-0.5, 0.5,-0.1}
            };
            m.tex_coords.clear();
            for(int i=0; i<6; ++i) m.tex_coords.push_back(UV_FULL);
            m.colliders.clear();
            m.colliders.push_back({glm::vec3(-0.5, -0.5, -0.1), glm::vec3(0.5, 0.5, 0.1)});
            m.shading_values = default_shading(6);
            return m;
        }
    }

    namespace Flat {
        ModelData get_model() {
            ModelData m; m.transparent = true; m.is_cube = false;
            m.vertex_positions = {{ 0.5,-0.4375, 0.5, 0.5,-0.4375,-0.5,-0.5,-0.4375,-0.5,-0.5,-0.4375, 0.5}, {-0.5,-0.4375, 0.5,-0.5,-0.4375,-0.5, 0.5,-0.4375,-0.5, 0.5,-0.4375, 0.5}};
            m.tex_coords = {UV_FULL, UV_FULL};
            m.shading_values = {{1.0,1.0,1.0,1.0}, {0.4,0.4,0.4,0.4}}; m.tex_coords_len = 2; return m;
        }
    }

    // ИСПРАВЛЕНО: Soil (Грядка)
    namespace Soil {
        ModelData get_model() {
            ModelData m = Cube::get_model();
            m.transparent = true;
            m.is_cube = false; // Важно, чтобы не рендерились соседи при стыковке

            // Коллизия чуть ниже (-0.5 до 0.4375)
            m.colliders.clear();
            m.colliders.push_back({glm::vec3(-0.5,-0.5,-0.5), glm::vec3(0.5,0.4375,0.5)});

            // Опускаем верхнюю грань (Top = index 2)
            // Y = 0.4375 (вместо 0.5)
            for(int i=0; i<4; i++) m.vertex_positions[2][i*3+1] = 0.4375f;

            // Опускаем верхние вершины боковых граней (0,1,4,5)
            // Проходим по всем вершинам и если Y == 0.5 -> Y = 0.4375
            for(auto& face : m.vertex_positions) {
                for(int i=0; i<4; i++) {
                    if(face[i*3+1] == 0.5f) face[i*3+1] = 0.4375f;
                }
            }

            // Коррекция UV для боковых граней (чтобы не растягивались)
            // Высота = 15/16 = 0.9375. Текстура должна идти от 0.0 до 0.9375 по V
            float h = 0.9375f;
            std::vector<float> uv_side = {0.0, h,  0.0, 0.0,  1.0, 0.0,  1.0, h};

            m.tex_coords[0] = uv_side; // Right
            m.tex_coords[1] = uv_side; // Left
            m.tex_coords[4] = uv_side; // Front
            m.tex_coords[5] = uv_side; // Back
            // Top (2) и Bottom (3) остаются FULL

            return m;
        }
    }

    namespace Crop {
        ModelData get_model() {
            ModelData m; m.transparent = true; m.is_cube = false;
            m.vertex_positions = {
                { 0.25, 0.4375, 0.50, 0.25,-0.5625, 0.50, 0.25,-0.5625,-0.50, 0.25, 0.4375,-0.50},
                { 0.25, 0.4375,-0.50, 0.25,-0.5625,-0.50, 0.25,-0.5625, 0.50, 0.25, 0.4375, 0.50},
                {-0.25, 0.4375,-0.50,-0.25,-0.5625,-0.50,-0.25,-0.5625, 0.50,-0.25, 0.4375, 0.50},
                {-0.25, 0.4375, 0.50,-0.25,-0.5625, 0.50,-0.25,-0.5625,-0.50,-0.25, 0.4375,-0.50},
                {-0.50, 0.4375, 0.25,-0.50,-0.5625, 0.25, 0.50,-0.5625, 0.25, 0.50, 0.4375, 0.25},
                { 0.50, 0.4375, 0.25, 0.50,-0.5625, 0.25,-0.50,-0.5625, 0.25,-0.50, 0.4375, 0.25},
                { 0.50, 0.4375,-0.25, 0.50,-0.5625,-0.25,-0.50,-0.5625,-0.25,-0.50, 0.4375,-0.25},
                {-0.50, 0.4375,-0.25,-0.50,-0.5625,-0.25, 0.50,-0.5625,-0.25, 0.50, 0.4375,-0.25}
            };
            for(int i=0; i<8; ++i) m.tex_coords.push_back(UV_FULL);
            m.shading_values = default_shading(8); m.tex_coords_len = 8; return m;
        }
    }

    namespace Cactus {
        ModelData get_model() {
            ModelData m; m.transparent = true; m.is_cube = false;
            m.colliders.push_back({glm::vec3(-0.4375,-0.5,-0.4375), glm::vec3(0.4375,0.5,0.4375)});
            m.vertex_positions = {
                { 0.4375, 0.5, 0.5, 0.4375,-0.5, 0.5, 0.4375,-0.5,-0.5, 0.4375, 0.5,-0.5},
                {-0.4375, 0.5,-0.5,-0.4375,-0.5,-0.5,-0.4375,-0.5, 0.5,-0.4375, 0.5, 0.5},
                { 0.5, 0.5, 0.5, 0.5, 0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5, 0.5},
                {-0.5,-0.5, 0.5,-0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,-0.5, 0.5},
                {-0.5, 0.5, 0.4375,-0.5,-0.5, 0.4375, 0.5,-0.5, 0.4375, 0.5, 0.5, 0.4375},
                { 0.5, 0.5,-0.4375, 0.5,-0.5,-0.4375,-0.5,-0.5,-0.4375,-0.5, 0.5,-0.4375}
            };
            for(int i=0; i<6; ++i) m.tex_coords.push_back(UV_FULL);
            m.shading_values = default_shading(6); return m;
        }
    }

    namespace Stairs { ModelData get_model() { return Slab::get_model(); } }
}
