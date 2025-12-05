#pragma once
#include "inventory.h"
#include <glm/glm.hpp>

class Player;
class World;
class TextRenderer;
class BlockPreviewRenderer;
class UiQuadRenderer;

class InventoryUI {
public:
    InventoryUI(Player* player, World* world, TextRenderer* text, BlockPreviewRenderer* preview, UiQuadRenderer* quad);

    void set_screen_size(int w, int h);
    void toggle();
    void open();
    void close(bool drop_cursor);
    bool is_open() const { return open_flag; }

    void handle_mouse_button(int button, int action, double mx, double my, int mods = 0);
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
    float slot_size = 56.0f;
    float slot_padding = 8.0f;

    glm::vec2 panel_origin() const;
    glm::vec2 panel_size() const;
    glm::vec2 slot_position(int index) const;
    bool point_in_rect(double x, double y, const glm::vec2& pos, const glm::vec2& size) const;
    int slot_from_point(double x, double y) const;
    void draw_slot(int index, bool hovered, bool selected);
    void draw_icon(const InventorySlot& slot, const glm::vec2& pos);
};
