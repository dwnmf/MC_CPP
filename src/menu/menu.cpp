#include "menu.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Menu {

SettingsMenu::SettingsMenu(TextRenderer* text_renderer, UiQuadRenderer* quad_renderer)
    : text_renderer(text_renderer)
    , quad_renderer(quad_renderer)
{
}

void SettingsMenu::set_screen_size(int width, int height) {
    screen_width = width;
    screen_height = height;
}

void SettingsMenu::add_category(const std::string& name) {
    MenuCategory category;
    category.name = name;
    categories.push_back(category);
}

void SettingsMenu::add_item(MenuItem item) {
    if (categories.empty()) {
        add_category("General");
    }
    categories.back().items.push_back(item);
}

void SettingsMenu::reset() {
    start_requested = false;
    quit_requested = false;
    current_category = 0;
    current_item = 0;
}

void SettingsMenu::handle_input(GLFWwindow* window, float dt) {
    // Update animation
    selection_pulse += dt * 3.0f;
    if (selection_pulse > 6.283f) selection_pulse -= 6.283f;
    
    if (quad_renderer) {
        quad_renderer->update(dt);
    }
    
    // Get current key states
    bool up = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || 
              glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool down = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || 
                glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool left = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || 
                glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool right = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || 
                 glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    bool tab = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || 
                 glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    bool enter = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || 
                 glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    
    // Edge detection for navigation (single press)
    if (up && !prev_up) {
        navigate_up();
    }
    if (down && !prev_down) {
        navigate_down();
    }
    
    // Tab switching for categories
    if (tab && !shift && !prev_tab) {
        switch_category(1);
    }
    if (tab && shift && !prev_shift_tab) {
        switch_category(-1);
    }
    
    // Left/Right for value changes - with hold repeat
    if (left || right) {
        key_repeat_timer += dt;
        bool should_change = false;
        
        if ((left && !prev_left) || (right && !prev_right)) {
            // First press
            should_change = true;
            key_repeat_timer = 0.0f;
            key_held = true;
        } else if (key_held && key_repeat_timer > key_repeat_delay) {
            // Holding - repeat at rate
            should_change = true;
            key_repeat_timer = key_repeat_delay - key_repeat_rate;
        }
        
        if (should_change) {
            if (left) navigate_left();
            if (right) navigate_right();
        }
    } else {
        key_repeat_timer = 0.0f;
        key_held = false;
    }
    
    // Enter/Space to start game
    if (enter && !prev_enter) {
        select_item();
    }
    
    // Escape to quit
    if (escape) {
        quit_requested = true;
    }
    
    // Update previous states
    prev_up = up;
    prev_down = down;
    prev_left = left;
    prev_right = right;
    prev_tab = tab && !shift;
    prev_shift_tab = tab && shift;
    prev_enter = enter;
}

void SettingsMenu::navigate_up() {
    if (categories.empty() || categories[current_category].items.empty()) return;
    
    int new_item = find_next_interactive(current_item, -1);
    if (new_item != -1) {
        current_item = new_item;
    }
}

void SettingsMenu::navigate_down() {
    if (categories.empty() || categories[current_category].items.empty()) return;
    
    int new_item = find_next_interactive(current_item, 1);
    if (new_item != -1) {
        current_item = new_item;
    }
}

void SettingsMenu::navigate_left() {
    if (categories.empty() || categories[current_category].items.empty()) return;
    
    auto& items = categories[current_category].items;
    if (current_item >= 0 && current_item < static_cast<int>(items.size())) {
        items[current_item].change_value(-1);
    }
}

void SettingsMenu::navigate_right() {
    if (categories.empty() || categories[current_category].items.empty()) return;
    
    auto& items = categories[current_category].items;
    if (current_item >= 0 && current_item < static_cast<int>(items.size())) {
        items[current_item].change_value(1);
    }
}

void SettingsMenu::switch_category(int direction) {
    if (categories.empty()) return;
    
    current_category += direction;
    if (current_category < 0) current_category = static_cast<int>(categories.size()) - 1;
    if (current_category >= static_cast<int>(categories.size())) current_category = 0;
    
    // Reset item selection and find first interactive
    current_item = find_next_interactive(-1, 1);
    if (current_item == -1) current_item = 0;
}

void SettingsMenu::select_item() {
    if (categories.empty() || categories[current_category].items.empty()) return;
    
    auto& items = categories[current_category].items;
    if (current_item >= 0 && current_item < static_cast<int>(items.size())) {
        auto& item = items[current_item];
        
        // Toggle for bool items
        if (item.type == MenuItemType::TOGGLE) {
            item.toggle();
        }
        // Execute for buttons
        else if (item.type == MenuItemType::BUTTON) {
            item.execute();
        }
    }
    
    // Start game on Enter if not on a button
    start_requested = true;
}

int SettingsMenu::find_next_interactive(int from, int direction) {
    if (categories.empty() || categories[current_category].items.empty()) return -1;
    
    auto& items = categories[current_category].items;
    int count = static_cast<int>(items.size());
    int pos = from + direction;
    
    // Search in direction
    while (pos >= 0 && pos < count) {
        if (items[pos].is_interactive()) {
            return pos;
        }
        pos += direction;
    }
    
    // Wrap around
    if (direction > 0) {
        for (int i = 0; i < count; ++i) {
            if (items[i].is_interactive()) return i;
        }
    } else {
        for (int i = count - 1; i >= 0; --i) {
            if (items[i].is_interactive()) return i;
        }
    }
    
    return from;  // Stay at current if no interactive items found
}

void SettingsMenu::render() {
    if (!text_renderer) return;
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    render_title();
    render_category_tabs();
    render_items();
    render_controls_hint();
    
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void SettingsMenu::render_title() {
    // Title with shadow effect
    text_renderer->RenderText("MC-CPP", MARGIN_LEFT + 2.0f, TITLE_Y + 2.0f, 1.8f, glm::vec3(0.1f));
    text_renderer->RenderText("MC-CPP", MARGIN_LEFT, TITLE_Y, 1.8f, glm::vec3(1.0f, 0.95f, 0.9f));
}

void SettingsMenu::render_category_tabs() {
    if (categories.empty()) return;
    
    float x = MARGIN_LEFT;
    float y = CATEGORY_TAB_Y;
    
    for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
        bool selected = (i == current_category);
        
        // Calculate text width (approximate)
        float text_width = categories[i].name.length() * 12.0f;
        float tab_width = text_width + 30.0f;
        float tab_height = 32.0f;
        
        // Draw tab background
        if (quad_renderer) {
            UiQuadStyle style;
            if (selected) {
                style.color_top = glm::vec4(0.28f, 0.22f, 0.15f, 0.95f);
                style.color_bottom = glm::vec4(0.18f, 0.14f, 0.08f, 0.95f);
                style.border_color = glm::vec4(1.0f, 0.75f, 0.3f, 0.9f);
                style.border_width = 2.0f;
            } else {
                style.color_top = glm::vec4(0.12f, 0.12f, 0.15f, 0.7f);
                style.color_bottom = glm::vec4(0.08f, 0.08f, 0.10f, 0.7f);
                style.border_color = glm::vec4(0.3f, 0.3f, 0.35f, 0.5f);
                style.border_width = 1.0f;
            }
            style.corner_radius = 6.0f;
            quad_renderer->draw_styled_quad(glm::vec2(x - 10.0f, y - 5.0f), 
                                            glm::vec2(tab_width, tab_height), style);
        }
        
        // Draw tab text
        glm::vec3 color = selected ? glm::vec3(1.0f, 0.9f, 0.75f) : glm::vec3(0.6f, 0.6f, 0.65f);
        text_renderer->RenderText(categories[i].name, x, y, 0.9f, color);
        
        x += tab_width + 15.0f;
    }
}

void SettingsMenu::render_items() {
    if (categories.empty() || current_category >= static_cast<int>(categories.size())) return;
    
    auto& items = categories[current_category].items;
    float y = ITEMS_START_Y;
    
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        auto& item = items[i];
        bool selected = (i == current_item);
        
        // Selection highlight
        if (selected && item.is_interactive()) {
            float pulse = 0.5f + 0.1f * std::sin(selection_pulse);
            
            if (quad_renderer) {
                UiQuadStyle style;
                style.color_top = glm::vec4(0.25f, 0.2f, 0.12f, 0.35f * pulse);
                style.color_bottom = glm::vec4(0.18f, 0.14f, 0.08f, 0.35f * pulse);
                style.border_color = glm::vec4(1.0f, 0.75f, 0.3f, 0.5f);
                style.border_width = 1.5f;
                style.corner_radius = 4.0f;
                
                quad_renderer->draw_styled_quad(
                    glm::vec2(MARGIN_LEFT - 20.0f, y - 5.0f),
                    glm::vec2(screen_width - MARGIN_LEFT * 2 + 40.0f, ITEM_HEIGHT - 5.0f),
                    style
                );
            }
        }
        
        // Selection arrow
        if (selected && item.is_interactive()) {
            text_renderer->RenderText(">", MARGIN_LEFT - 20.0f, y, 1.0f, glm::vec3(1.0f, 0.75f, 0.3f));
        }
        
        // Label color based on type and selection
        glm::vec3 label_color;
        if (item.type == MenuItemType::LABEL) {
            label_color = glm::vec3(0.5f, 0.5f, 0.55f);  // Dimmer for headers
        } else if (selected) {
            label_color = glm::vec3(1.0f, 1.0f, 1.0f);
        } else {
            label_color = glm::vec3(0.85f, 0.85f, 0.9f);
        }
        
        // Draw label
        text_renderer->RenderText(item.label, MARGIN_LEFT, y, 1.0f, label_color);
        
        // Draw value based on type
        float value_x = MARGIN_LEFT + VALUE_OFFSET;
        
        switch (item.type) {
            case MenuItemType::SLIDER: {
                // Calculate fill percentage
                float current_val = item.float_value ? *item.float_value : 
                                   (item.int_value ? static_cast<float>(*item.int_value) : 0.0f);
                float fill = (current_val - item.min_val) / (item.max_val - item.min_val);
                fill = std::clamp(fill, 0.0f, 1.0f);
                
                // Draw slider bar
                render_slider_bar(value_x, y, SLIDER_WIDTH, fill, selected);
                
                // Draw value text
                std::string val_str = item.get_value_string();
                text_renderer->RenderText(val_str, value_x + SLIDER_WIDTH + 15.0f, y, 0.9f, 
                                         selected ? glm::vec3(1.0f, 0.95f, 0.85f) : glm::vec3(0.7f, 0.7f, 0.75f));
                break;
            }
            
            case MenuItemType::TOGGLE: {
                std::string val_str = item.get_value_string();
                glm::vec3 val_color;
                if (item.bool_value && *item.bool_value) {
                    val_color = glm::vec3(0.4f, 1.0f, 0.5f);  // Green for ON
                } else {
                    val_color = glm::vec3(1.0f, 0.4f, 0.4f);  // Red for OFF
                }
                
                // Draw toggle box
                if (quad_renderer) {
                    UiQuadStyle style;
                    bool is_on = item.bool_value && *item.bool_value;
                    if (is_on) {
                        style.color_top = glm::vec4(0.15f, 0.4f, 0.2f, 0.9f);
                        style.color_bottom = glm::vec4(0.1f, 0.25f, 0.12f, 0.9f);
                        style.border_color = glm::vec4(0.3f, 1.0f, 0.4f, 0.8f);
                    } else {
                        style.color_top = glm::vec4(0.35f, 0.15f, 0.15f, 0.9f);
                        style.color_bottom = glm::vec4(0.2f, 0.1f, 0.1f, 0.9f);
                        style.border_color = glm::vec4(1.0f, 0.3f, 0.3f, 0.8f);
                    }
                    style.corner_radius = 4.0f;
                    style.border_width = selected ? 2.0f : 1.5f;
                    
                    quad_renderer->draw_styled_quad(glm::vec2(value_x, y - 2.0f), 
                                                    glm::vec2(60.0f, 26.0f), style);
                }
                
                text_renderer->RenderText(val_str, value_x + 12.0f, y, 0.9f, val_color);
                break;
            }
            
            case MenuItemType::SELECT: {
                std::string val_str = "< " + item.get_value_string() + " >";
                glm::vec3 val_color = selected ? glm::vec3(1.0f, 0.95f, 0.85f) : glm::vec3(0.75f, 0.75f, 0.8f);
                text_renderer->RenderText(val_str, value_x, y, 0.9f, val_color);
                break;
            }
            
            case MenuItemType::BUTTON: {
                // Draw button box
                if (quad_renderer) {
                    UiQuadStyle style;
                    if (selected) {
                        style.color_top = glm::vec4(0.28f, 0.22f, 0.15f, 0.95f);
                        style.color_bottom = glm::vec4(0.18f, 0.14f, 0.08f, 0.95f);
                        style.border_color = glm::vec4(1.0f, 0.75f, 0.3f, 0.9f);
                        style.glow_intensity = 0.3f;
                    } else {
                        style.color_top = glm::vec4(0.18f, 0.18f, 0.22f, 0.85f);
                        style.color_bottom = glm::vec4(0.1f, 0.1f, 0.14f, 0.85f);
                        style.border_color = glm::vec4(0.4f, 0.4f, 0.5f, 0.7f);
                    }
                    style.corner_radius = 6.0f;
                    style.border_width = selected ? 2.0f : 1.5f;
                    
                    float btn_width = item.label.length() * 12.0f + 40.0f;
                    quad_renderer->draw_styled_quad(glm::vec2(MARGIN_LEFT - 15.0f, y - 5.0f), 
                                                    glm::vec2(btn_width, 32.0f), style);
                }
                break;
            }
            
            default:
                break;
        }
        
        y += ITEM_HEIGHT;
    }
}

void SettingsMenu::render_slider_bar(float x, float y, float width, float fill_percent, bool selected) {
    if (!quad_renderer) return;
    
    float bar_height = 20.0f;
    float bar_y = y;
    
    // Background
    UiQuadStyle bg_style;
    bg_style.color_top = glm::vec4(0.08f, 0.08f, 0.1f, 0.9f);
    bg_style.color_bottom = glm::vec4(0.05f, 0.05f, 0.07f, 0.9f);
    bg_style.border_color = glm::vec4(0.3f, 0.3f, 0.35f, 0.6f);
    bg_style.corner_radius = 4.0f;
    bg_style.border_width = 1.0f;
    
    quad_renderer->draw_styled_quad(glm::vec2(x, bar_y), glm::vec2(width, bar_height), bg_style);
    
    // Fill bar
    if (fill_percent > 0.01f) {
        UiQuadStyle fill_style;
        if (selected) {
            fill_style.color_top = glm::vec4(0.9f, 0.65f, 0.25f, 0.95f);
            fill_style.color_bottom = glm::vec4(0.75f, 0.5f, 0.15f, 0.95f);
        } else {
            fill_style.color_top = glm::vec4(0.7f, 0.55f, 0.3f, 0.85f);
            fill_style.color_bottom = glm::vec4(0.55f, 0.4f, 0.2f, 0.85f);
        }
        fill_style.border_color = glm::vec4(0.0f);
        fill_style.corner_radius = 3.0f;
        fill_style.border_width = 0.0f;
        
        float fill_width = std::max(6.0f, (width - 4.0f) * fill_percent);
        quad_renderer->draw_styled_quad(glm::vec2(x + 2.0f, bar_y + 2.0f), 
                                        glm::vec2(fill_width, bar_height - 4.0f), fill_style);
    }
}

void SettingsMenu::render_controls_hint() {
    float y = static_cast<float>(screen_height) - 60.0f;
    
    // Background panel for hints
    if (quad_renderer) {
        UiQuadStyle style;
        style.color_top = glm::vec4(0.06f, 0.06f, 0.08f, 0.8f);
        style.color_bottom = glm::vec4(0.03f, 0.03f, 0.05f, 0.85f);
        style.border_color = glm::vec4(0.3f, 0.3f, 0.4f, 0.5f);
        style.corner_radius = 8.0f;
        style.border_width = 1.0f;
        
        quad_renderer->draw_styled_quad(glm::vec2(MARGIN_LEFT - 20.0f, y - 15.0f), 
                                        glm::vec2(screen_width - MARGIN_LEFT * 2 + 40.0f, 75.0f), style);
    }
    
    // Controls text
    glm::vec3 hint_color(0.65f, 0.65f, 0.7f);
    glm::vec3 key_color(0.9f, 0.9f, 0.95f);
    
    std::string line1 = "[W/S] or [Up/Down]: Navigate   [A/D] or [Left/Right]: Change Value   [Tab]: Switch Category";
    std::string line2 = "[Enter/Space]: Start Game   [Esc]: Quit";
    
    text_renderer->RenderText(line1, MARGIN_LEFT, y, 0.75f, hint_color);
    text_renderer->RenderText(line2, MARGIN_LEFT, y + 25.0f, 0.75f, hint_color);
}

} // namespace Menu
