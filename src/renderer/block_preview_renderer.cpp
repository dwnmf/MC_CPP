#include "block_preview_renderer.h"
#include "texture_manager.h"
#include "shader.h"
#include "block_type.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

BlockPreviewRenderer::BlockPreviewRenderer(TextureManager* tm) : texture_manager(tm) {
    shader = std::make_unique<Shader>("assets/shaders/ui/block_preview_vert.glsl", "assets/shaders/ui/block_preview_frag.glsl");
    if (!shader->valid()) {
        std::cout << "Failed to load block preview shader." << std::endl;
    }
}

BlockPreviewRenderer::~BlockPreviewRenderer() {
    for (auto& kv : meshes) {
        destroy_mesh(kv.second);
    }
}

void BlockPreviewRenderer::destroy_mesh(Mesh& m) {
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.vao) glDeleteVertexArrays(1, &m.vao);
    m.vbo = 0; m.vao = 0; m.vertex_count = 0;
}

void BlockPreviewRenderer::build_mesh(int block_id, const BlockType* type) {
    if (!type || meshes.count(block_id)) return;

    if (type->tex_coords.empty() || type->shading_values.empty()) return;

    Mesh mesh;
    std::vector<Vertex> verts;
    verts.reserve(type->vertex_positions.size() * 6);

    for (size_t face = 0; face < type->vertex_positions.size(); ++face) {
        const auto& pos = type->vertex_positions[face];
        if (pos.size() < 12) continue;
        const auto& uv = (face < type->tex_coords.size()) ? type->tex_coords[face] : type->tex_coords.front();
        const auto& shading = (face < type->shading_values.size()) ? type->shading_values[face] : type->shading_values.front();
        int layer = 0;
        if (face < type->tex_indices.size()) layer = type->tex_indices[face];

        glm::vec3 p[4];
        glm::vec3 t[4];
        float shade[4];
        for (int i = 0; i < 4; ++i) {
            p[i] = glm::vec3(pos[i * 3 + 0], pos[i * 3 + 1], pos[i * 3 + 2]);
            t[i] = glm::vec3(uv[i * 2 + 0], uv[i * 2 + 1], static_cast<float>(layer));
            shade[i] = (i < static_cast<int>(shading.size())) ? shading[i] : 1.0f;
        }

        int idx[6] = {0, 1, 2, 0, 2, 3};
        for (int i : idx) {
            verts.push_back({p[i], t[i], shade[i]});
        }
    }

    if (verts.empty()) return;

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3) * 2));

    glBindVertexArray(0);
    mesh.vertex_count = static_cast<GLsizei>(verts.size());
    meshes[block_id] = mesh;
}

void BlockPreviewRenderer::draw_block(int block_id, const BlockType* type, const glm::mat4& model, const glm::mat4& view_proj, const glm::vec3& light_dir, float brightness) {
    if (!shader || !shader->valid() || !texture_manager || !type) return;
    if (meshes.count(block_id) == 0) build_mesh(block_id, type);
    auto it = meshes.find(block_id);
    if (it == meshes.end()) return;
    const Mesh& mesh = it->second;
    if (mesh.vertex_count == 0) return;

    shader->use();
    int modelLoc = shader->find_uniform("u_Model");
    int vpLoc = shader->find_uniform("u_ViewProj");
    int lightLoc = shader->find_uniform("u_LightDir");
    int brightLoc = shader->find_uniform("u_Brightness");
    int samplerLoc = shader->find_uniform("u_TextureArraySampler");

    if (modelLoc >= 0) shader->setMat4(modelLoc, model);
    if (vpLoc >= 0) shader->setMat4(vpLoc, view_proj);
    if (lightLoc >= 0) shader->setVec3(lightLoc, light_dir);
    if (brightLoc >= 0) shader->setFloat(brightLoc, brightness);
    if (samplerLoc >= 0) shader->setInt(samplerLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_manager->texture_array);

    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);
    glBindVertexArray(0);
}
