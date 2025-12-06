#pragma once
#include <string>
#include <vector>
#include <functional>

namespace Menu {

// Types of menu items
enum class MenuItemType {
    LABEL,      // Text header (non-interactive)
    SLIDER,     // Slider for int/float values
    TOGGLE,     // On/Off toggle for bool values
    SELECT,     // Selection from a list of options
    BUTTON      // Action button
};

// Menu item structure
struct MenuItem {
    std::string label;
    MenuItemType type = MenuItemType::LABEL;
    
    // For SLIDER type
    float* float_value = nullptr;
    int* int_value = nullptr;
    float min_val = 0.0f;
    float max_val = 100.0f;
    float step = 1.0f;
    
    // For TOGGLE type
    bool* bool_value = nullptr;
    
    // For SELECT type
    std::vector<std::string> options;
    int* selected_index = nullptr;
    std::vector<int> option_values;  // Optional: actual values for each option
    
    // For BUTTON type
    std::function<void()> on_click = nullptr;
    
    // Helper methods
    bool is_interactive() const {
        return type != MenuItemType::LABEL;
    }
    
    // Get current value as string (for display)
    std::string get_value_string() const;
    
    // Change value by delta (for slider/select)
    void change_value(int direction);
    
    // Toggle value (for toggle type)
    void toggle();
    
    // Execute button action
    void execute();
};

// Category containing menu items
struct MenuCategory {
    std::string name;
    std::vector<MenuItem> items;
};

} // namespace Menu
