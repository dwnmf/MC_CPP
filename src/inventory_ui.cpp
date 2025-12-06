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

// ============================================================================
// Construction & Screen Size
// ============================================================================

InventoryUI::InventoryUI(Player* p, World* w, TextRenderer* text, BlockPreviewRenderer* preview, UiQuadRenderer* quad)
: player(p), world(w), text_renderer(text), preview_renderer(preview), quad_renderer(quad) {
    // Initialize slot appear progress to 0
    for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
        anim.slot_appear_progress[i] = 0.0f;
    }
}

void InventoryUI::set_screen_size(int w, int h) {
    screen_w = w;
    screen_h = h;
    if (quad_renderer) quad_renderer->set_screen_size(w, h);
}

// ============================================================================
// Layout Calculations
// ============================================================================

glm::vec2 InventoryUI::panel_size() const {
    float width = slot_size * 9 + slot_padding * 8 + panel_padding * 2;
    float title_h = 36.0f;
    float separator_h = 12.0f;
    float height = panel_padding * 2 + title_h + 3 * (slot_size + slot_padding) + separator_h + slot_size;
    return glm::vec2(width, height);
}

glm::vec2 InventoryUI::panel_origin() const {
    glm::vec2 size = panel_size();
    return glm::vec2((screen_w - size.x) * 0.5f, (screen_h - size.y) * 0.5f);
}

glm::vec2 InventoryUI::slot_position(int index) const {
    glm::vec2 origin = panel_origin();
    float title_h = 36.0f;
    float separator_h = 12.0f;
    float grid_start_y = origin.y + panel_padding + title_h;
    float grid_start_x = origin.x + panel_padding;

    if (index < Inventory::HOTBAR_SIZE) {
        // Hotbar is at the bottom
        float y = grid_start_y + 3 * (slot_size + slot_padding) + separator_h;
        return glm::vec2(grid_start_x + index * (slot_size + slot_padding), y);
    }

    // Main inventory grid (3 rows)
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

// ============================================================================
// State Management
// ============================================================================

void InventoryUI::toggle() {
    if (open_flag) {
        close(false);
    } else {
        open();
    }
}

void InventoryUI::open() {
    open_flag = true;
    // Reset slot animations for staggered effect
    for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
        anim.slot_appear_progress[i] = 0.0f;
    }
}

void InventoryUI::close(bool drop_cursor) {
    open_flag = false;
    if (drop_cursor) drop_cursor_to_world();
    tooltip.visible = false;
}

// ============================================================================
// Animation
// ============================================================================

void InventoryUI::update(float delta_time) {
    anim.time += delta_time;
    
    // Update quad renderer time for shader animations
    if (quad_renderer) {
        quad_renderer->update(delta_time);
    }
    
    // Animate open/close
    float target_open = open_flag ? 1.0f : 0.0f;
    anim.open_progress += (target_open - anim.open_progress) * std::min(1.0f, delta_time * ANIMATION_SPEED);
    
    // Clamp to avoid floating point issues
    if (std::abs(anim.open_progress - target_open) < 0.001f) {
        anim.open_progress = target_open;
    }
    
    // Staggered slot appear animation when opening
    if (open_flag) {
        for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
            float delay = i * SLOT_APPEAR_DELAY;
            float time_since_open = anim.time;  // Simplified - ideally track open time
            if (time_since_open > delay && anim.slot_appear_progress[i] < 1.0f) {
                anim.slot_appear_progress[i] += delta_time * ANIMATION_SPEED * 1.5f;
                if (anim.slot_appear_progress[i] > 1.0f) anim.slot_appear_progress[i] = 1.0f;
            }
        }
    }
}

float InventoryUI::ease_out_cubic(float t) const {
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

float InventoryUI::ease_out_back(float t) const {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

// ============================================================================
// Input Handling
// ============================================================================

void InventoryUI::handle_mouse_button(int button, int action, double mx, double my, int mods) {
    if (!open_flag || action != GLFW_PRESS || !player) return;
    int slot_idx = slot_from_point(mx, my);
    if (slot_idx < 0) {
        if (!cursor.empty()) drop_cursor_to_world();
        return;
    }

    // Shift+Click = quick transfer between hotbar and inventory
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

// ============================================================================
// Tooltip
// ============================================================================

std::string InventoryUI::get_block_name(int block_id) const {
    if (!world || block_id <= 0 || block_id >= static_cast<int>(world->block_types.size())) {
        return "Unknown";
    }
    auto* bt = world->block_types[block_id];
    if (!bt) return "Unknown";
    
    // Try to get a readable name - for now, return generic names
    // In a full implementation, blocks would have display names
    static const char* names[] = {
        "Air", "Stone", "Grass", "Dirt", "Cobblestone", "Planks", 
        "Sapling", "Bedrock", "Water", "Water", "Lava", "Lava",
        "Sand", "Gravel", "Gold Ore", "Iron Ore", "Coal Ore", "Wood",
        "Leaves", "Sponge", "Glass", "Lapis Ore", "Lapis Block", "Dispenser",
        "Sandstone", "Note Block", "Bed", "Powered Rail", "Detector Rail", "Sticky Piston",
        "Cobweb", "Tall Grass", "Dead Bush", "Piston", "Piston Head", "Wool"
    };
    
    if (block_id < static_cast<int>(sizeof(names) / sizeof(names[0]))) {
        return names[block_id];
    }
    return "Block #" + std::to_string(block_id);
}

void InventoryUI::update_tooltip(int hovered_slot, double cursor_x, double cursor_y) {
    if (hovered_slot < 0 || !player) {
        tooltip.visible = false;
        return;
    }
    
    const InventorySlot& slot = player->inventory.slot(hovered_slot);
    if (slot.empty()) {
        tooltip.visible = false;
        return;
    }
    
    tooltip.visible = true;
    tooltip.slot_index = hovered_slot;
    tooltip.title = get_block_name(slot.id);
    tooltip.description = "Count: " + std::to_string(slot.count);
    
    // Position tooltip near cursor, but keep on screen
    float tooltip_width = 150.0f;
    float tooltip_height = 50.0f;
    float offset_x = 15.0f;
    float offset_y = 15.0f;
    
    tooltip.position.x = static_cast<float>(cursor_x) + offset_x;
    tooltip.position.y = static_cast<float>(cursor_y) + offset_y;
    
    // Keep on screen
    if (tooltip.position.x + tooltip_width > screen_w) {
        tooltip.position.x = static_cast<float>(cursor_x) - tooltip_width - 5.0f;
    }
    if (tooltip.position.y + tooltip_height > screen_h) {
        tooltip.position.y = static_cast<float>(cursor_y) - tooltip_height - 5.0f;
    }
}

void InventoryUI::draw_tooltip(double cursor_x, double cursor_y) {
    if (!tooltip.visible || !quad_renderer || !text_renderer) return;
    
    float padding = 8.0f;
    float title_height = 18.0f;
    float desc_height = 14.0f;
    float width = 140.0f;
    float height = padding * 2 + title_height + desc_height;
    
    quad_renderer->draw_tooltip(tooltip.position, glm::vec2(width, height));
    
    // Title
    text_renderer->RenderText(tooltip.title, 
        tooltip.position.x + padding, 
        tooltip.position.y + padding + 12.0f, 
        0.6f, glm::vec3(1.0f, 0.95f, 0.8f));
    
    // Description (smaller, dimmer)
    text_renderer->RenderText(tooltip.description,
        tooltip.position.x + padding,
        tooltip.position.y + padding + title_height + 10.0f,
        0.45f, glm::vec3(0.7f, 0.7f, 0.75f));
}

// ============================================================================
// Rendering
// ============================================================================

void InventoryUI::draw_icon(const InventorySlot& slot, const glm::vec2& pos, float scale) {
    if (!preview_renderer || !world || slot.empty()) return;
    if (slot.id <= 0 || slot.id >= static_cast<int>(world->block_types.size())) return;
    auto* bt = world->block_types[slot.id];
    if (!bt) return;

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screen_w), static_cast<float>(screen_h), 0.0f, -100.0f, 100.0f);
    glm::mat4 model(1.0f);
    glm::vec2 center = pos + glm::vec2(slot_size * 0.5f, slot_size * 0.55f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
    model = glm::scale(model, glm::vec3(slot_size * 0.42f * scale));
    model = glm::rotate(model, glm::radians(-32.0f), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(36.0f), glm::vec3(0, 1, 0));

    preview_renderer->draw_block(slot.id, bt, model, proj, glm::normalize(glm::vec3(-0.3f, -1.0f, -0.6f)), 1.0f);

    // Draw count with shadow
    if (slot.count > 1 && text_renderer) {
        std::string count = std::to_string(slot.count);
        float text_scale = 0.65f;
        float text_x = pos.x + slot_size - 14.0f - static_cast<float>(count.size()) * 7.0f;
        float text_y = pos.y + slot_size - 4.0f;
        
        // Shadow
        text_renderer->RenderText(count, text_x + 1.5f, text_y + 1.5f, text_scale, glm::vec3(0.0f));
        // Text
        text_renderer->RenderText(count, text_x, text_y, text_scale, glm::vec3(1.0f));
    }
}

void InventoryUI::draw_slot_with_animation(int index, bool hovered, bool selected, float appear_progress) {
    if (!quad_renderer || !player) return;
    
    float eased = ease_out_back(appear_progress);
    float scale = 0.5f + 0.5f * eased;
    
    glm::vec2 base_pos = slot_position(index);
    glm::vec2 size(slot_size * scale, slot_size * scale);
    glm::vec2 pos = base_pos + glm::vec2(slot_size * (1.0f - scale) * 0.5f);
    
    bool slot_empty = player->inventory.slot(index).empty();
    quad_renderer->draw_slot(pos, size, hovered, selected, slot_empty);
    
    if (appear_progress > 0.5f) {
        draw_icon(player->inventory.slot(index), base_pos, scale);
    }
}

void InventoryUI::draw_slot(int index, bool hovered, bool selected) {
    draw_slot_with_animation(index, hovered, selected, 1.0f);
}

void InventoryUI::draw_separator(float y) {
    if (!quad_renderer) return;
    
    glm::vec2 origin = panel_origin();
    glm::vec2 size = panel_size();
    
    float sep_padding = 30.0f;
    float sep_width = size.x - sep_padding * 2;
    
    // Draw gradient separator line
    UiQuadStyle style;
    style.color_top = glm::vec4(0.4f, 0.4f, 0.5f, 0.4f);
    style.color_bottom = glm::vec4(0.4f, 0.4f, 0.5f, 0.4f);
    style.border_width = 0.0f;
    style.corner_radius = 1.0f;
    
    quad_renderer->draw_styled_quad(
        glm::vec2(origin.x + sep_padding, y),
        glm::vec2(sep_width, 2.0f),
        style
    );
}

void InventoryUI::draw_panel_background(float alpha) {
    if (!quad_renderer) return;
    
    glm::vec2 origin = panel_origin();
    glm::vec2 size = panel_size();
    
    // Apply fade animation
    UiQuadStyle border_style = UiPresets::PanelBorder();
    border_style.color_top.a *= alpha;
    border_style.color_bottom.a *= alpha;
    border_style.border_color.a *= alpha;
    
    UiQuadStyle bg_style = UiPresets::PanelBackground();
    bg_style.color_top.a *= alpha;
    bg_style.color_bottom.a *= alpha;
    bg_style.border_color.a *= alpha;
    
    // Outer glow/border
    quad_renderer->draw_styled_quad(origin - glm::vec2(4.0f), size + glm::vec2(8.0f), border_style);
    
    // Main panel
    quad_renderer->draw_styled_quad(origin, size, bg_style);
}

void InventoryUI::draw_hotbar() {
    if (!quad_renderer || !player) return;
    
    float padding = 12.0f;
    float total_width = slot_size * 9 + slot_padding * 8;
    float start_x = (screen_w - total_width) * 0.5f;
    float y = screen_h - slot_size - padding;
    
    // Hotbar background
    UiQuadStyle hotbar_bg = UiPresets::HotbarBackground();
    float bg_padding = 8.0f;
    quad_renderer->draw_styled_quad(
        glm::vec2(start_x - bg_padding, y - bg_padding),
        glm::vec2(total_width + bg_padding * 2, slot_size + bg_padding * 2),
        hotbar_bg
    );
    
    // Draw slots
    for (int i = 0; i < Inventory::HOTBAR_SIZE; ++i) {
        glm::vec2 pos(start_x + i * (slot_size + slot_padding), y);
        bool selected = (player->hotbar_selected == i);
        
        quad_renderer->draw_hotbar_slot(pos, glm::vec2(slot_size), selected);
        draw_icon(player->inventory.slot(i), pos);
        
        // Slot number
        if (text_renderer) {
            std::string num = std::to_string(i + 1);
            float num_x = pos.x + 5.0f;
            float num_y = pos.y + 14.0f;
            // Shadow
            text_renderer->RenderText(num, num_x + 1.0f, num_y + 1.0f, 0.45f, glm::vec3(0.0f, 0.0f, 0.0f));
            // Number
            glm::vec3 num_color = selected ? glm::vec3(1.0f, 0.85f, 0.4f) : glm::vec3(0.6f, 0.6f, 0.65f);
            text_renderer->RenderText(num, num_x, num_y, 0.45f, num_color);
        }
    }
}

void InventoryUI::render(double cursor_x, double cursor_y) {
    if (!quad_renderer) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw inventory panel when open (or animating)
    if (open_flag || anim.open_progress > 0.01f) {
        float alpha = anim.open_progress;
        
        // Draw panel background with fade
        draw_panel_background(alpha);

        // Title
        if (text_renderer && alpha > 0.5f) {
            glm::vec2 origin = panel_origin();
            // Shadow
            text_renderer->RenderText("Inventory", origin.x + panel_padding + 1.0f, origin.y + panel_padding + 20.0f, 0.85f, glm::vec3(0.0f));
            // Title with gradient-like effect (bright center)
            text_renderer->RenderText("Inventory", origin.x + panel_padding, origin.y + panel_padding + 19.0f, 0.85f, glm::vec3(0.95f, 0.9f, 0.85f));
        }
        
        // Find hovered slot
        int hovered = slot_from_point(cursor_x, cursor_y);
        
        // Draw separator between main inventory and hotbar
        glm::vec2 origin = panel_origin();
        float title_h = 36.0f;
        float sep_y = origin.y + panel_padding + title_h + 3 * (slot_size + slot_padding) - slot_padding * 0.5f + 3.0f;
        draw_separator(sep_y);
        
        // Draw main inventory slots (top 3 rows)
        for (int i = Inventory::HOTBAR_SIZE; i < Inventory::TOTAL_SLOTS; ++i) {
            float appear = anim.slot_appear_progress[i] * alpha;
            draw_slot_with_animation(i, hovered == i, false, appear);
        }
        
        // Draw hotbar slots in inventory panel
        for (int i = 0; i < Inventory::HOTBAR_SIZE; ++i) {
            bool selected = (player && player->hotbar_selected == i);
            float appear = anim.slot_appear_progress[i] * alpha;
            draw_slot_with_animation(i, hovered == i, selected, appear);
        }
        
        // Update and draw tooltip
        update_tooltip(hovered, cursor_x, cursor_y);
        if (alpha > 0.8f) {
            draw_tooltip(cursor_x, cursor_y);
        }
    }

    // Always draw hotbar at bottom when closed
    if (!open_flag && anim.open_progress < 0.1f) {
        draw_hotbar();
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

// ============================================================================
// Item Dropping
// ============================================================================

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
