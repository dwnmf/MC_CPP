#pragma once
#include "menu_item.h"
#include "../text_renderer.h"
#include "../renderer/ui_quad_renderer.h"
#include <GLFW/glfw3.h>
#include <vector>

namespace Menu {

class SettingsMenu {
public:
    SettingsMenu(TextRenderer* text_renderer, UiQuadRenderer* quad_renderer);
    ~SettingsMenu() = default;
    
    // Screen size management
    void set_screen_size(int width, int height);
    
    // Category and item management
    void add_category(const std::string& name);
    void add_item(MenuItem item);
    
    // Input and rendering
    void handle_input(GLFWwindow* window, float dt);
    void render();
    
    // State queries
    bool is_start_requested() const { return start_requested; }
    bool is_quit_requested() const { return quit_requested; }
    
    // Reset state for re-opening menu
    void reset();

private:
    // Renderers
    TextRenderer* text_renderer;
    UiQuadRenderer* quad_renderer;
    
    // Screen dimensions
    int screen_width = 1366;
    int screen_height = 768;
    
    // Menu data
    std::vector<MenuCategory> categories;
    int current_category = 0;
    int current_item = 0;
    
    // State
    bool start_requested = false;
    bool quit_requested = false;
    
    // Input debouncing
    float key_repeat_timer = 0.0f;
    float key_repeat_delay = 0.15f;  // Initial delay before repeat
    float key_repeat_rate = 0.08f;   // Rate of repeat
    bool key_held = false;
    
    // Animation
    float selection_pulse = 0.0f;
    float category_transition = 0.0f;
    
    // Key states for edge detection
    bool prev_up = false;
    bool prev_down = false;
    bool prev_left = false;
    bool prev_right = false;
    bool prev_tab = false;
    bool prev_shift_tab = false;
    bool prev_enter = false;
    
    // Layout constants
    static constexpr float TITLE_Y = 40.0f;
    static constexpr float CATEGORY_TAB_Y = 100.0f;
    static constexpr float ITEMS_START_Y = 160.0f;
    static constexpr float ITEM_HEIGHT = 40.0f;
    static constexpr float MARGIN_LEFT = 60.0f;
    static constexpr float VALUE_OFFSET = 350.0f;
    static constexpr float SLIDER_WIDTH = 200.0f;
    
    // Helper methods
    void navigate_up();
    void navigate_down();
    void navigate_left();
    void navigate_right();
    void switch_category(int direction);
    void select_item();
    
    // Rendering helpers
    void render_title();
    void render_category_tabs();
    void render_items();
    void render_controls_hint();
    void render_slider_bar(float x, float y, float width, float fill_percent, bool selected);
    
    // Find next interactive item (skip labels)
    int find_next_interactive(int from, int direction);
};

} // namespace Menu
