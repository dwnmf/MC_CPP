#pragma once
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"

class UiQuadRenderer {
public:
    UiQuadRenderer();
    ~UiQuadRenderer();

    void set_screen_size(int width, int height);
    void draw_quad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    glm::mat4 proj{1.0f};
    std::unique_ptr<Shader> shader;
};
