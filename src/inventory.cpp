#include "inventory.h"
#include <algorithm>

// Статический null_slot для безопасного возврата при неверном индексе
InventorySlot Inventory::null_slot;

Inventory::Inventory() : slots(TOTAL_SLOTS) {}

bool Inventory::in_bounds(int index) const {
    return index >= 0 && index < static_cast<int>(slots.size());
}

// Безопасный доступ к слоту - возвращает nullptr при неверном индексе
InventorySlot* Inventory::slot_safe(int index) {
    if (!in_bounds(index)) return nullptr;
    return &slots[index];
}

const InventorySlot* Inventory::slot_safe(int index) const {
    if (!in_bounds(index)) return nullptr;
    return &slots[index];
}

// Прямой доступ с проверкой границ (для обратной совместимости)
InventorySlot& Inventory::slot(int index) {
    if (!in_bounds(index)) {
        null_slot.clear(); // Сброс заглушки
        return null_slot;
    }
    return slots[index];
}

const InventorySlot& Inventory::slot(int index) const {
    if (!in_bounds(index)) {
        return null_slot;
    }
    return slots[index];
}

// Быстрое перемещение предмета (Shift+Click)
// Из хотбара (0-8) -> в основной инвентарь (9-35)
// Из основного инвентаря (9-35) -> в хотбар (0-8)
void Inventory::quick_transfer(int index) {
    if (!in_bounds(index)) return;
    InventorySlot& src = slots[index];
    if (src.empty()) return;

    int start, end;
    if (index < HOTBAR_SIZE) {
        // Из хотбара в основной инвентарь
        start = HOTBAR_SIZE;
        end = TOTAL_SLOTS;
    } else {
        // Из основного инвентаря в хотбар
        start = 0;
        end = HOTBAR_SIZE;
    }

    int remaining = src.count;
    int id = src.id;
    int meta = src.metadata;

    // Сначала пытаемся добавить в существующие стаки
    for (int i = start; i < end && remaining > 0; ++i) {
        InventorySlot& dst = slots[i];
        if (!dst.empty() && dst.id == id && dst.metadata == meta && dst.count < MAX_STACK) {
            int space = MAX_STACK - dst.count;
            int move = std::min(space, remaining);
            dst.count += move;
            remaining -= move;
        }
    }

    // Затем ищем пустые слоты
    for (int i = start; i < end && remaining > 0; ++i) {
        InventorySlot& dst = slots[i];
        if (dst.empty()) {
            dst.id = id;
            dst.metadata = meta;
            int move = std::min(MAX_STACK, remaining);
            dst.count = move;
            remaining -= move;
        }
    }

    // Обновляем исходный слот
    if (remaining <= 0) {
        src.clear();
    } else {
        src.count = remaining;
    }
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
