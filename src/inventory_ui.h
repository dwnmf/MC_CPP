#pragma once
#include "inventory.h"
#include <glm/glm.hpp>
#include <string>

class Player;
class World;
class TextRenderer;
class BlockPreviewRenderer;
class UiQuadRenderer;

// Animation state for smooth transitions
struct UIAnimationState {
    float open_progress = 0.0f;      // 0 = closed, 1 = fully open
    float slot_appear_progress[Inventory::TOTAL_SLOTS] = {0.0f};
    float hovered_slot_glow = 0.0f;
    int last_hovered_slot = -1;
    float time = 0.0f;
};

// Tooltip data
struct TooltipData {
    bool visible = false;
    glm::vec2 position{0.0f};
    std::string title;
    std::string description;
    int slot_index = -1;
};

class InventoryUI {
public:
    InventoryUI(Player* player, World* world, TextRenderer* text, BlockPreviewRenderer* preview, UiQuadRenderer* quad);

    void set_screen_size(int w, int h);
    void toggle();
    void open();
    void close(bool drop_cursor);
    bool is_open() const { return open_flag; }

    void handle_mouse_button(int button, int action, double mx, double my, int mods = 0);
    void update(float delta_time);  // New: Update animations
    void render(double cursor_x, double cursor_y);
    void drop_cursor_to_world();

    InventorySlot& cursor_slot() { return cursor; }
    const InventorySlot& cursor_slot() const { return cursor; }

private:
    Player* player;
    World* world;
    TextRenderer* text_renderer;
    BlockPreviewRenderer* preview_renderer;
    UiQuadRenderer* quad_renderer;

    InventorySlot cursor;
    bool open_flag = false;
    int screen_w = 1280;
    int screen_h = 720;
    
    // UI sizing
    float slot_size = 60.0f;        // Increased from 56
    float slot_padding = 10.0f;     // Increased from 8
    float panel_padding = 16.0f;    // Panel internal padding
    
    // Animation state
    UIAnimationState anim;
    TooltipData tooltip;
    
    // Style constants
    static constexpr float ANIMATION_SPEED = 8.0f;
    static constexpr float SLOT_APPEAR_DELAY = 0.02f;  // Staggered animation delay
    static constexpr float HOVER_GLOW_SPEED = 10.0f;
    
    // Helper methods
    glm::vec2 panel_origin() const;
    glm::vec2 panel_size() const;
    glm::vec2 slot_position(int index) const;
    bool point_in_rect(double x, double y, const glm::vec2& pos, const glm::vec2& size) const;
    int slot_from_point(double x, double y) const;
    
    // Rendering methods
    void draw_slot(int index, bool hovered, bool selected);
    void draw_slot_with_animation(int index, bool hovered, bool selected, float appear_progress);
    void draw_icon(const InventorySlot& slot, const glm::vec2& pos, float scale = 1.0f);
    void draw_panel_background(float alpha);
    void draw_hotbar();
    void draw_tooltip(double cursor_x, double cursor_y);
    void draw_separator(float y);
    
    // Tooltip helpers
    void update_tooltip(int hovered_slot, double cursor_x, double cursor_y);
    std::string get_block_name(int block_id) const;
    
    // Animation helpers
    float ease_out_cubic(float t) const;
    float ease_out_back(float t) const;
};
