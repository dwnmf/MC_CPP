#include "block_type.h"
#include <vector>
#include <string>

BlockType::BlockType(TextureManager* tm, std::string name, std::map<std::string, std::string> face_tex, ModelData md)
: name(name), model(md), block_face_textures(face_tex) {

    transparent = md.transparent;
    is_cube = md.is_cube;
    glass = md.glass;
    translucent = md.translucent;

    for (const auto& c : md.colliders) colliders.push_back(Collider(c.first, c.second));

    vertex_positions = md.vertex_positions;
    tex_coords = md.tex_coords;
    shading_values = md.shading_values;
    tex_indices.resize(md.tex_coords_len, 0);

    // Helper lambda to set texture index
    auto apply_texture = [&](const std::string& key, auto func) {
        if (face_tex.count(key)) {
            std::string texture = face_tex.at(key);
            tm->add_texture(texture);
            int idx = tm->get_texture_index(texture);
            func(idx);
        }
    };

    auto set_face = [&](int face, int idx) {
        if (face < tex_indices.size()) tex_indices[face] = idx;
    };

    // 1. Применяем общие текстуры (низкий приоритет)
    apply_texture("all", [&](int idx) {
        for(size_t i=0; i<tex_indices.size(); ++i) set_face(i, idx);
    });

    // 2. Применяем группы сторон
    apply_texture("sides", [&](int idx) {
        set_face(0, idx); set_face(1, idx); set_face(4, idx); set_face(5, idx);
    });
    apply_texture("x", [&](int idx) { set_face(0, idx); set_face(1, idx); });
    apply_texture("y", [&](int idx) { set_face(2, idx); set_face(3, idx); });
    apply_texture("z", [&](int idx) { set_face(4, idx); set_face(5, idx); });

    // 3. Применяем конкретные стороны (высокий приоритет, перезаписывают предыдущие)
    apply_texture("right",  [&](int idx) { set_face(0, idx); });
    apply_texture("left",   [&](int idx) { set_face(1, idx); });
    apply_texture("top",    [&](int idx) { set_face(2, idx); });
    apply_texture("bottom", [&](int idx) { set_face(3, idx); });
    apply_texture("front",  [&](int idx) { set_face(4, idx); });
    apply_texture("back",   [&](int idx) { set_face(5, idx); });
}
