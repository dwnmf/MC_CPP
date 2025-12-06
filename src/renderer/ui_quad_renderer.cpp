#include "ui_quad_renderer.h"
#include <glm/gtc/matrix_transform.hpp>

UiQuadRenderer::UiQuadRenderer() {
    // Legacy simple shader
    shader = std::make_unique<Shader>("assets/shaders/ui/quad_vert.glsl", "assets/shaders/ui/quad_frag.glsl");
    
    // Modern rounded shader with effects
    rounded_shader = std::make_unique<Shader>("assets/shaders/ui/quad_rounded_vert.glsl", "assets/shaders/ui/quad_rounded_frag.glsl");

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

void UiQuadRenderer::update(float delta_time) {
    time += delta_time;
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

void UiQuadRenderer::draw_styled_quad(const glm::vec2& pos, const glm::vec2& size, const UiQuadStyle& style) {
    if (!rounded_shader || !rounded_shader->valid()) {
        // Fallback to legacy if rounded shader unavailable
        draw_quad(pos, size, style.color_top);
        return;
    }
    
    rounded_shader->use();
    rounded_shader->setMat4(rounded_shader->find_uniform("u_Proj"), proj);
    rounded_shader->setVec2(rounded_shader->find_uniform("u_Pos"), pos);
    rounded_shader->setVec2(rounded_shader->find_uniform("u_Size"), size);
    
    // Colors
    rounded_shader->setVec4(rounded_shader->find_uniform("u_ColorTop"), style.color_top);
    rounded_shader->setVec4(rounded_shader->find_uniform("u_ColorBottom"), style.color_bottom);
    rounded_shader->setVec4(rounded_shader->find_uniform("u_BorderColor"), style.border_color);
    rounded_shader->setVec4(rounded_shader->find_uniform("u_GlowColor"), style.glow_color);
    
    // Parameters
    rounded_shader->setFloat(rounded_shader->find_uniform("u_CornerRadius"), style.corner_radius);
    rounded_shader->setFloat(rounded_shader->find_uniform("u_BorderWidth"), style.border_width);
    rounded_shader->setFloat(rounded_shader->find_uniform("u_GlowIntensity"), style.glow_intensity);
    rounded_shader->setFloat(rounded_shader->find_uniform("u_Time"), time);
    
    // Animation flags
    rounded_shader->setInt(rounded_shader->find_uniform("u_PulseEnabled"), style.pulse_enabled ? 1 : 0);
    rounded_shader->setInt(rounded_shader->find_uniform("u_ShineEnabled"), style.shine_enabled ? 1 : 0);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void UiQuadRenderer::draw_slot(const glm::vec2& pos, const glm::vec2& size, bool hovered, bool selected, bool empty) {
    UiQuadStyle style;
    
    if (selected) {
        style = UiPresets::SlotSelected();
    } else if (hovered) {
        style = UiPresets::SlotHovered();
    } else if (empty) {
        style = UiPresets::SlotEmpty();
    } else {
        style = UiPresets::SlotDefault();
    }
    
    draw_styled_quad(pos, size, style);
}

void UiQuadRenderer::draw_panel(const glm::vec2& pos, const glm::vec2& size) {
    // Draw outer border first
    UiQuadStyle border_style = UiPresets::PanelBorder();
    draw_styled_quad(pos - glm::vec2(3.0f), size + glm::vec2(6.0f), border_style);
    
    // Draw inner panel
    UiQuadStyle panel_style = UiPresets::PanelBackground();
    draw_styled_quad(pos, size, panel_style);
}

void UiQuadRenderer::draw_hotbar_slot(const glm::vec2& pos, const glm::vec2& size, bool selected) {
    UiQuadStyle style;
    
    if (selected) {
        style = UiPresets::SlotSelected();
        style.corner_radius = 8.0f;
    } else {
        style = UiPresets::SlotDefault();
        style.corner_radius = 8.0f;
        style.color_top.a = 0.8f;
        style.color_bottom.a = 0.85f;
    }
    
    draw_styled_quad(pos, size, style);
}

void UiQuadRenderer::draw_tooltip(const glm::vec2& pos, const glm::vec2& size) {
    draw_styled_quad(pos, size, UiPresets::Tooltip());
}
