#include "menu_item.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Menu {

std::string MenuItem::get_value_string() const {
    switch (type) {
        case MenuItemType::SLIDER: {
            std::stringstream ss;
            if (float_value) {
                // Format with 1 decimal place for floats
                if (step < 1.0f) {
                    ss << std::fixed << std::setprecision(2) << *float_value;
                } else {
                    ss << static_cast<int>(*float_value);
                }
            } else if (int_value) {
                ss << *int_value;
            }
            return ss.str();
        }
        
        case MenuItemType::TOGGLE: {
            if (bool_value) {
                return *bool_value ? "ON" : "OFF";
            }
            return "N/A";
        }
        
        case MenuItemType::SELECT: {
            if (selected_index && *selected_index >= 0 && 
                *selected_index < static_cast<int>(options.size())) {
                return options[*selected_index];
            }
            return "N/A";
        }
        
        case MenuItemType::BUTTON:
            return "";
            
        case MenuItemType::LABEL:
        default:
            return "";
    }
}

void MenuItem::change_value(int direction) {
    switch (type) {
        case MenuItemType::SLIDER: {
            if (float_value) {
                *float_value += direction * step;
                if (*float_value < min_val) *float_value = min_val;
                if (*float_value > max_val) *float_value = max_val;
            } else if (int_value) {
                *int_value += direction * static_cast<int>(step);
                if (*int_value < static_cast<int>(min_val)) *int_value = static_cast<int>(min_val);
                if (*int_value > static_cast<int>(max_val)) *int_value = static_cast<int>(max_val);
            }
            break;
        }
        
        case MenuItemType::SELECT: {
            if (selected_index && !options.empty()) {
                *selected_index += direction;
                int opt_count = static_cast<int>(options.size());
                if (*selected_index < 0) *selected_index = opt_count - 1;
                if (*selected_index >= opt_count) *selected_index = 0;
                
                // Apply actual value if option_values is provided
                if (!option_values.empty() && int_value && 
                    *selected_index < static_cast<int>(option_values.size())) {
                    *int_value = option_values[*selected_index];
                }
            }
            break;
        }
        
        case MenuItemType::TOGGLE: {
            // For toggle, any direction change toggles the value
            toggle();
            break;
        }
        
        default:
            break;
    }
}

void MenuItem::toggle() {
    if (type == MenuItemType::TOGGLE && bool_value) {
        *bool_value = !(*bool_value);
    }
}

void MenuItem::execute() {
    if (type == MenuItemType::BUTTON && on_click) {
        on_click();
    }
}

} // namespace Menu
