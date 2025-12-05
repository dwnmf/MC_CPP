#include "ui_quad_renderer.h"
#include <glm/gtc/matrix_transform.hpp>

UiQuadRenderer::UiQuadRenderer() {
    shader = std::make_unique<Shader>("assets/shaders/ui/quad_vert.glsl", "assets/shaders/ui/quad_frag.glsl");

    float verts[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

UiQuadRenderer::~UiQuadRenderer() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void UiQuadRenderer::set_screen_size(int width, int height) {
    proj = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
}

void UiQuadRenderer::draw_quad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) {
    if (!shader || !shader->valid()) return;
    shader->use();
    shader->setMat4(shader->find_uniform("u_Proj"), proj);
    shader->setVec2(shader->find_uniform("u_Pos"), pos);
    shader->setVec2(shader->find_uniform("u_Size"), size);
    shader->setVec3(shader->find_uniform("u_Color"), glm::vec3(color));
    int alphaLoc = shader->find_uniform("u_Alpha");
    if (alphaLoc >= 0) shader->setFloat(alphaLoc, color.a);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
