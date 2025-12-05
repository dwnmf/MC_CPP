#pragma once
#include <vector>

struct InventorySlot {
    int id = 0;
    int count = 0;
    int metadata = 0;

    bool empty() const { return id == 0 || count <= 0; }
    void clear() { id = 0; count = 0; metadata = 0; }
};

class Inventory {
public:
    static constexpr int HOTBAR_SIZE = 9;
    static constexpr int MAIN_ROWS = 3;
    static constexpr int MAIN_COLUMNS = 9;
    static constexpr int TOTAL_SLOTS = HOTBAR_SIZE + MAIN_ROWS * MAIN_COLUMNS;
    static constexpr int MAX_STACK = 64;

    Inventory();

    int add_item(int id, int amount, int metadata = 0);
    int add_item_to_slot(int index, int id, int amount, int metadata = 0);
    int remove_from_slot(int index, int amount);

    InventorySlot& slot(int index) { return slots[index]; }
    const InventorySlot& slot(int index) const { return slots[index]; }
    const std::vector<InventorySlot>& data() const { return slots; }

private:
    bool in_bounds(int index) const;
    std::vector<InventorySlot> slots;
};
