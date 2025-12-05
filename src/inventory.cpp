#include "inventory.h"
#include <algorithm>

Inventory::Inventory() : slots(TOTAL_SLOTS) {}

bool Inventory::in_bounds(int index) const {
    return index >= 0 && index < static_cast<int>(slots.size());
}

int Inventory::add_item_to_slot(int index, int id, int amount, int metadata) {
    if (!in_bounds(index) || amount <= 0 || id == 0) return amount;
    InventorySlot& s = slots[index];

    if (!s.empty() && (s.id != id || s.metadata != metadata)) return amount;

    if (s.empty()) {
        s.id = id;
        s.metadata = metadata;
        s.count = 0;
    }

    int space = MAX_STACK - s.count;
    int to_place = std::min(space, amount);
    s.count += to_place;
    return amount - to_place;
}

int Inventory::add_item(int id, int amount, int metadata) {
    if (id == 0 || amount <= 0) return amount;

    // 1. Fill existing partial stacks
    for (int i = 0; i < static_cast<int>(slots.size()) && amount > 0; ++i) {
        InventorySlot& s = slots[i];
        if (!s.empty() && s.id == id && s.metadata == metadata && s.count < MAX_STACK) {
            amount = add_item_to_slot(i, id, amount, metadata);
        }
    }

    // 2. Use empty slots
    for (int i = 0; i < static_cast<int>(slots.size()) && amount > 0; ++i) {
        if (slots[i].empty()) {
            amount = add_item_to_slot(i, id, amount, metadata);
        }
    }

    return amount; // leftover
}

int Inventory::remove_from_slot(int index, int amount) {
    if (!in_bounds(index) || amount <= 0) return 0;
    InventorySlot& s = slots[index];
    if (s.empty()) return 0;

    int removed = std::min(amount, s.count);
    s.count -= removed;
    if (s.count <= 0) s.clear();
    return removed;
}
