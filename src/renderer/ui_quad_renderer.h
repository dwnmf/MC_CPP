#pragma once
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"

// Style configuration for modern UI elements
struct UiQuadStyle {
    glm::vec4 color_top{0.15f, 0.15f, 0.18f, 0.9f};      // Top gradient color
    glm::vec4 color_bottom{0.08f, 0.08f, 0.10f, 0.9f};   // Bottom gradient color
    glm::vec4 border_color{0.5f, 0.5f, 0.55f, 0.6f};     // Border color
    glm::vec4 glow_color{0.0f, 0.0f, 0.0f, 0.0f};        // Glow effect color
    float corner_radius = 8.0f;                           // Corner rounding
    float border_width = 1.5f;                            // Border thickness
    float glow_intensity = 0.0f;                          // Glow effect strength
    bool pulse_enabled = false;                           // Pulse animation
    bool shine_enabled = false;                           // Shine animation
};

// Preset styles for common UI states
namespace UiPresets {
    // Slot styles
    inline UiQuadStyle SlotDefault() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.12f, 0.12f, 0.14f, 0.85f);
        s.color_bottom = glm::vec4(0.06f, 0.06f, 0.08f, 0.85f);
        s.border_color = glm::vec4(0.35f, 0.35f, 0.40f, 0.5f);
        s.corner_radius = 6.0f;
        s.border_width = 1.5f;
        return s;
    }
    
    inline UiQuadStyle SlotHovered() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.18f, 0.20f, 0.25f, 0.92f);
        s.color_bottom = glm::vec4(0.10f, 0.12f, 0.16f, 0.92f);
        s.border_color = glm::vec4(0.3f, 0.85f, 0.95f, 0.9f);  // Cyan accent
        s.glow_color = glm::vec4(0.2f, 0.8f, 0.9f, 0.4f);
        s.corner_radius = 6.0f;
        s.border_width = 2.0f;
        s.glow_intensity = 0.6f;
        return s;
    }
    
    inline UiQuadStyle SlotSelected() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.22f, 0.18f, 0.12f, 0.95f);
        s.color_bottom = glm::vec4(0.14f, 0.10f, 0.06f, 0.95f);
        s.border_color = glm::vec4(1.0f, 0.75f, 0.2f, 0.95f);  // Gold accent
        s.glow_color = glm::vec4(1.0f, 0.7f, 0.1f, 0.5f);
        s.corner_radius = 6.0f;
        s.border_width = 2.5f;
        s.glow_intensity = 0.8f;
        s.pulse_enabled = true;
        return s;
    }
    
    inline UiQuadStyle SlotEmpty() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.08f, 0.08f, 0.10f, 0.6f);
        s.color_bottom = glm::vec4(0.04f, 0.04f, 0.06f, 0.6f);
        s.border_color = glm::vec4(0.25f, 0.25f, 0.30f, 0.35f);
        s.corner_radius = 6.0f;
        s.border_width = 1.0f;
        return s;
    }
    
    // Panel styles
    inline UiQuadStyle PanelBackground() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.08f, 0.08f, 0.12f, 0.88f);
        s.color_bottom = glm::vec4(0.04f, 0.04f, 0.06f, 0.92f);
        s.border_color = glm::vec4(0.4f, 0.4f, 0.5f, 0.6f);
        s.corner_radius = 12.0f;
        s.border_width = 2.0f;
        return s;
    }
    
    inline UiQuadStyle PanelBorder() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.20f, 0.20f, 0.25f, 0.7f);
        s.color_bottom = glm::vec4(0.15f, 0.15f, 0.18f, 0.7f);
        s.border_color = glm::vec4(0.5f, 0.5f, 0.6f, 0.8f);
        s.corner_radius = 14.0f;
        s.border_width = 3.0f;
        return s;
    }
    
    // Hotbar styles
    inline UiQuadStyle HotbarBackground() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.06f, 0.06f, 0.10f, 0.85f);
        s.color_bottom = glm::vec4(0.03f, 0.03f, 0.05f, 0.9f);
        s.border_color = glm::vec4(0.35f, 0.35f, 0.45f, 0.7f);
        s.corner_radius = 10.0f;
        s.border_width = 2.0f;
        return s;
    }

    // Tooltip style
    inline UiQuadStyle Tooltip() {
        UiQuadStyle s;
        s.color_top = glm::vec4(0.12f, 0.12f, 0.18f, 0.95f);
        s.color_bottom = glm::vec4(0.06f, 0.06f, 0.10f, 0.95f);
        s.border_color = glm::vec4(0.5f, 0.5f, 0.7f, 0.8f);
        s.corner_radius = 8.0f;
        s.border_width = 1.5f;
        return s;
    }
}

class UiQuadRenderer {
public:
    UiQuadRenderer();
    ~UiQuadRenderer();

    void set_screen_size(int width, int height);
    
    // Legacy method (for backwards compatibility)
    void draw_quad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);
    
    // Modern styled quad with all effects
    void draw_styled_quad(const glm::vec2& pos, const glm::vec2& size, const UiQuadStyle& style);
    
    // Convenience methods for common UI elements
    void draw_slot(const glm::vec2& pos, const glm::vec2& size, bool hovered, bool selected, bool empty = false);
    void draw_panel(const glm::vec2& pos, const glm::vec2& size);
    void draw_hotbar_slot(const glm::vec2& pos, const glm::vec2& size, bool selected);
    void draw_tooltip(const glm::vec2& pos, const glm::vec2& size);
    
    // Update animation time (call once per frame)
    void update(float delta_time);
    
    // Current animation time
    float get_time() const { return time; }

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    glm::mat4 proj{1.0f};
    
    std::unique_ptr<Shader> shader;          // Legacy shader
    std::unique_ptr<Shader> rounded_shader;  // Modern shader with effects
    
    float time = 0.0f;  // Animation time
};
