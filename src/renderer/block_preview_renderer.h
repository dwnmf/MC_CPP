#pragma once
#include <unordered_map>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"

class TextureManager;
class BlockType;

class BlockPreviewRenderer {
public:
    explicit BlockPreviewRenderer(TextureManager* tm);
    ~BlockPreviewRenderer();

    void draw_block(int block_id, const BlockType* type, const glm::mat4& model, const glm::mat4& view_proj, const glm::vec3& light_dir, float brightness);
    Shader* get_shader() { return shader.get(); }

private:
    struct Mesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLsizei vertex_count = 0;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 uv;
        float shade;
    };

    TextureManager* texture_manager;
    std::unordered_map<int, Mesh> meshes;
    std::unique_ptr<Shader> shader;

    void build_mesh(int block_id, const BlockType* type);
    void destroy_mesh(Mesh& m);
};
