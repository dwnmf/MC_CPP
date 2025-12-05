#include "inventory_ui.h"
#include "entity/player.h"
#include "world.h"
#include "text_renderer.h"
#include "renderer/block_preview_renderer.h"
#include "renderer/ui_quad_renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <string>
#include <cmath>

InventoryUI::InventoryUI(Player* p, World* w, TextRenderer* text, BlockPreviewRenderer* preview, UiQuadRenderer* quad)
: player(p), world(w), text_renderer(text), preview_renderer(preview), quad_renderer(quad) {}

void InventoryUI::set_screen_size(int w, int h) {
    screen_w = w;
    screen_h = h;
    if (quad_renderer) quad_renderer->set_screen_size(w, h);
}

glm::vec2 InventoryUI::panel_size() const {
    float width = slot_size * 9 + slot_padding * 10;
    float height = slot_padding * 5 + 4 * slot_size + slot_padding * 3 + 40.0f; // title + gap + hotbar
    return glm::vec2(width, height);
}

glm::vec2 InventoryUI::panel_origin() const {
    glm::vec2 size = panel_size();
    return glm::vec2((screen_w - size.x) * 0.5f, (screen_h - size.y) * 0.5f);
}

glm::vec2 InventoryUI::slot_position(int index) const {
    glm::vec2 origin = panel_origin();
    float title_h = 30.0f;
    float grid_start_y = origin.y + slot_padding * 2 + title_h;
    float grid_start_x = origin.x + slot_padding * 2;

    if (index < Inventory::HOTBAR_SIZE) {
        float hotbar_gap = slot_padding * 3.0f;
        float y = grid_start_y + 3 * (slot_size + slot_padding) + hotbar_gap;
        return glm::vec2(grid_start_x + index * (slot_size + slot_padding), y);
    }

    int local = index - Inventory::HOTBAR_SIZE;
    int row = local / Inventory::MAIN_COLUMNS;
    int col = local % Inventory::MAIN_COLUMNS;
    float x = grid_start_x + col * (slot_size + slot_padding);
    float y = grid_start_y + row * (slot_size + slot_padding);
    return glm::vec2(x, y);
}

bool InventoryUI::point_in_rect(double x, double y, const glm::vec2& pos, const glm::vec2& size) const {
    return x >= pos.x && x <= pos.x + size.x && y >= pos.y && y <= pos.y + size.y;
}

int InventoryUI::slot_from_point(double x, double y) const {
    glm::vec2 size(slot_size, slot_size);
    for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
        glm::vec2 pos = slot_position(i);
        if (point_in_rect(x, y, pos, size)) return i;
    }
    return -1;
}

void InventoryUI::toggle() {
    open_flag = !open_flag;
}

void InventoryUI::open() {
    open_flag = true;
}

void InventoryUI::close(bool drop_cursor) {
    open_flag = false;
    if (drop_cursor) drop_cursor_to_world();
}

void InventoryUI::handle_mouse_button(int button, int action, double mx, double my, int mods) {
    if (!open_flag || action != GLFW_PRESS || !player) return;
    int slot_idx = slot_from_point(mx, my);
    if (slot_idx < 0) {
        if (!cursor.empty()) drop_cursor_to_world();
        return;
    }

    // Shift+Click = быстрое перемещение между хотбаром и инвентарём
    if ((mods & GLFW_MOD_SHIFT) && button == GLFW_MOUSE_BUTTON_LEFT) {
        player->inventory.quick_transfer(slot_idx);
        return;
    }

    InventorySlot& slot = player->inventory.slot(slot_idx);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (cursor.empty() && !slot.empty()) {
            cursor = slot;
            slot.clear();
        } else if (!cursor.empty() && slot.empty()) {
            slot = cursor;
            cursor.clear();
        } else if (!cursor.empty() && !slot.empty()) {
            if (slot.id == cursor.id && slot.metadata == cursor.metadata) {
                int space = Inventory::MAX_STACK - slot.count;
                int move = std::min(space, cursor.count);
                slot.count += move;
                cursor.count -= move;
                if (cursor.count <= 0) cursor.clear();
            } else {
                std::swap(cursor, slot);
            }
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (cursor.empty() && !slot.empty()) {
            int half = (slot.count + 1) / 2;
            cursor = slot;
            cursor.count = half;
            slot.count -= half;
            if (slot.count <= 0) slot.clear();
        } else if (!cursor.empty()) {
            if (slot.empty()) {
                slot.id = cursor.id;
                slot.metadata = cursor.metadata;
                slot.count = 1;
                cursor.count -= 1;
                if (cursor.count <= 0) cursor.clear();
            } else if (slot.id == cursor.id && slot.metadata == cursor.metadata && slot.count < Inventory::MAX_STACK) {
                slot.count += 1;
                cursor.count -= 1;
                if (cursor.count <= 0) cursor.clear();
            }
        }
    }
}

void InventoryUI::draw_icon(const InventorySlot& slot, const glm::vec2& pos) {
    if (!preview_renderer || !world || slot.empty()) return;
    if (slot.id <= 0 || slot.id >= static_cast<int>(world->block_types.size())) return;
    auto* bt = world->block_types[slot.id];
    if (!bt) return;

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screen_w), static_cast<float>(screen_h), 0.0f, -100.0f, 100.0f);
    glm::mat4 model(1.0f);
    glm::vec2 center = pos + glm::vec2(slot_size * 0.5f, slot_size * 0.55f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
    model = glm::scale(model, glm::vec3(slot_size * 0.4f));
    model = glm::rotate(model, glm::radians(-32.0f), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(36.0f), glm::vec3(0, 1, 0));

    preview_renderer->draw_block(slot.id, bt, model, proj, glm::normalize(glm::vec3(-0.3f, -1.0f, -0.6f)), 1.0f);

    if (slot.count > 1 && text_renderer) {
        std::string count = std::to_string(slot.count);
        float scale = 0.7f;
        float text_x = pos.x + slot_size - 18.0f - static_cast<float>(count.size()) * 6.0f;
        float text_y = pos.y + slot_size - 6.0f;
        text_renderer->RenderText(count, text_x + 1.0f, text_y + 1.0f, scale, glm::vec3(0.0f));
        text_renderer->RenderText(count, text_x, text_y, scale, glm::vec3(1.0f));
    }
}

void InventoryUI::draw_slot(int index, bool hovered, bool selected) {
    if (!quad_renderer || !player) return;
    glm::vec2 pos = slot_position(index);
    glm::vec2 size(slot_size, slot_size);

    glm::vec4 border_color = hovered ? glm::vec4(0.9f, 0.9f, 0.9f, 0.9f) : glm::vec4(0.75f, 0.75f, 0.75f, 0.6f);
    if (selected) border_color = glm::vec4(0.9f, 0.85f, 0.35f, 0.9f);
    glm::vec4 fill_color(0.08f, 0.08f, 0.08f, 0.8f);

    float border = 2.0f;
    quad_renderer->draw_quad(pos, size, border_color);
    quad_renderer->draw_quad(pos + glm::vec2(border), size - glm::vec2(border * 2.0f), fill_color);

    draw_icon(player->inventory.slot(index), pos);
}

void InventoryUI::render(double cursor_x, double cursor_y) {
    if (!quad_renderer) return;

    // Draw background and slots when open
    if (open_flag) {
        glm::vec2 origin = panel_origin();
        glm::vec2 size = panel_size();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        quad_renderer->draw_quad(origin - glm::vec2(4.0f), size + glm::vec2(8.0f), glm::vec4(0.25f, 0.25f, 0.25f, 0.8f));
        quad_renderer->draw_quad(origin, size, glm::vec4(0.06f, 0.06f, 0.06f, 0.75f));

        if (text_renderer) {
            text_renderer->RenderText("Inventory", origin.x + slot_padding * 2.0f, origin.y + 20.0f, 0.8f, glm::vec3(0.9f));
        }

        int hovered = slot_from_point(cursor_x, cursor_y);
        for (int i = Inventory::HOTBAR_SIZE; i < Inventory::TOTAL_SLOTS; ++i) {
            draw_slot(i, hovered == i, false);
        }
        for (int i = 0; i < Inventory::HOTBAR_SIZE; ++i) {
            bool selected = (player && player->hotbar_selected == i);
            draw_slot(i, hovered == i, selected);
        }
    }

    // Always draw hotbar outline at bottom when closed
    if (!open_flag && player) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float padding = 10.0f;
        float total_width = slot_size * 9 + slot_padding * 8;
        float start_x = (screen_w - total_width) * 0.5f;
        float y = screen_h - slot_size - padding;

        int hovered = -1;
        if (cursor_x >= start_x && cursor_x <= start_x + total_width &&
            cursor_y >= y && cursor_y <= y + slot_size) {
            int idx = static_cast<int>((cursor_x - start_x) / (slot_size + slot_padding));
            if (idx >= 0 && idx < Inventory::HOTBAR_SIZE) hovered = idx;
        }
        for (int i = 0; i < Inventory::HOTBAR_SIZE; ++i) {
            glm::vec2 pos(start_x + i * (slot_size + slot_padding), y);
            glm::vec4 border_color = glm::vec4(0.75f, 0.75f, 0.75f, 0.5f);
            if (player->hotbar_selected == i) border_color = glm::vec4(0.95f, 0.85f, 0.35f, 0.9f);
            quad_renderer->draw_quad(pos - glm::vec2(2.0f), glm::vec2(slot_size + 4.0f), glm::vec4(0.05f, 0.05f, 0.05f, 0.6f));
            quad_renderer->draw_quad(pos, glm::vec2(slot_size), border_color);
            quad_renderer->draw_quad(pos + glm::vec2(2.0f), glm::vec2(slot_size - 4.0f), glm::vec4(0.1f, 0.1f, 0.1f, 0.7f));
            draw_icon(player->inventory.slot(i), pos);
            bool hovered_slot = hovered == i;
            if (hovered_slot && text_renderer) {
                text_renderer->RenderText(std::to_string(i + 1), pos.x + 6.0f, pos.y + 14.0f, 0.6f, glm::vec3(0.8f));
            }
        }
    }

    // Ghost cursor item
    if (!cursor.empty() && preview_renderer) {
        glm::vec2 ghost_pos(cursor_x - slot_size * 0.4f, cursor_y - slot_size * 0.4f);
        draw_icon(cursor, ghost_pos);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void InventoryUI::drop_cursor_to_world() {
    if (!world || !player || cursor.empty()) return;
    glm::vec3 forward(std::cos(player->rotation.x), 0.0f, std::sin(player->rotation.x));
    if (glm::length2(forward) < 0.0001f) forward = glm::vec3(0, 0, 1);
    forward = glm::normalize(forward);
    glm::vec3 start = player->position + glm::vec3(0.0f, player->eyelevel, 0.0f);
    glm::vec3 drop_pos = start + forward * 1.2f;

    world->spawn_item_entity(drop_pos, cursor.id, cursor.count, cursor.metadata);
    cursor.clear();
}
