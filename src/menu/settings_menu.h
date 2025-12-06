#pragma once
#include "menu.h"
#include "../options.h"
#include "../audio.h"

namespace Menu {

// Static storage for values that need separate variable (like audio volume)
struct SettingsStorage {
    static inline float audio_volume = 0.5f;
    static inline int antialiasing_index = 0;      // 0=Off, 1=2x, 2=4x, 3=8x
    static inline int shadow_quality_index = 2;    // 0=512, 1=1024, 2=2048, 3=4096
    static inline int mipmap_index = 2;            // 0=Nearest, 1=Linear, 2=Nearest Mipmap Linear, 3=Linear Mipmap Linear
    
    static void init() {
        audio_volume = Audio::GetVolume();
        
        // Map antialiasing value to index
        switch (Options::ANTIALIASING) {
            case 0: antialiasing_index = 0; break;
            case 2: antialiasing_index = 1; break;
            case 4: antialiasing_index = 2; break;
            case 8: antialiasing_index = 3; break;
            default: antialiasing_index = 0; break;
        }
        
        // Map shadow resolution to index
        switch (Options::SHADOW_MAP_RESOLUTION) {
            case 512:  shadow_quality_index = 0; break;
            case 1024: shadow_quality_index = 1; break;
            case 2048: shadow_quality_index = 2; break;
            case 4096: shadow_quality_index = 3; break;
            default:   shadow_quality_index = 2; break;
        }
        
        // Map mipmap type to index
        if (Options::MIPMAP_TYPE == GL_NEAREST) mipmap_index = 0;
        else if (Options::MIPMAP_TYPE == GL_LINEAR) mipmap_index = 1;
        else if (Options::MIPMAP_TYPE == GL_NEAREST_MIPMAP_LINEAR) mipmap_index = 2;
        else if (Options::MIPMAP_TYPE == GL_LINEAR_MIPMAP_LINEAR) mipmap_index = 3;
        else mipmap_index = 2;
    }
    
    static void apply() {
        Audio::SetVolume(audio_volume);
        
        // Map index back to antialiasing value
        static const int aa_values[] = {0, 2, 4, 8};
        Options::ANTIALIASING = aa_values[antialiasing_index];
        
        // Map index back to shadow resolution
        static const int shadow_values[] = {512, 1024, 2048, 4096};
        Options::SHADOW_MAP_RESOLUTION = shadow_values[shadow_quality_index];
        
        // Map index back to mipmap type
        static const int mipmap_values[] = {GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR};
        Options::MIPMAP_TYPE = mipmap_values[mipmap_index];
    }
};

inline void configure_settings_menu(SettingsMenu& menu) {
    // Initialize storage with current values
    SettingsStorage::init();
    
    // ====== GRAPHICS ======
    menu.add_category("Graphics");
    
    MenuItem render_dist;
    render_dist.label = "Render Distance";
    render_dist.type = MenuItemType::SLIDER;
    render_dist.int_value = &Options::RENDER_DISTANCE;
    render_dist.min_val = 2;
    render_dist.max_val = 32;
    render_dist.step = 1;
    menu.add_item(render_dist);
    
    MenuItem fov;
    fov.label = "Field of View";
    fov.type = MenuItemType::SLIDER;
    fov.float_value = &Options::FOV;
    fov.min_val = 30.0f;
    fov.max_val = 120.0f;
    fov.step = 5.0f;
    menu.add_item(fov);
    
    MenuItem vsync;
    vsync.label = "VSync";
    vsync.type = MenuItemType::TOGGLE;
    vsync.bool_value = &Options::VSYNC;
    menu.add_item(vsync);
    
    MenuItem antialiasing;
    antialiasing.label = "Antialiasing";
    antialiasing.type = MenuItemType::SELECT;
    antialiasing.options = {"Off", "2x", "4x", "8x"};
    antialiasing.selected_index = &SettingsStorage::antialiasing_index;
    antialiasing.option_values = {0, 2, 4, 8};
    antialiasing.int_value = &Options::ANTIALIASING;
    menu.add_item(antialiasing);
    
    MenuItem mipmap;
    mipmap.label = "Texture Filtering";
    mipmap.type = MenuItemType::SELECT;
    mipmap.options = {"Nearest", "Bilinear", "Trilinear", "Anisotropic"};
    mipmap.selected_index = &SettingsStorage::mipmap_index;
    mipmap.option_values = {GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR};
    mipmap.int_value = &Options::MIPMAP_TYPE;
    menu.add_item(mipmap);
    
    // ====== LIGHTING ======
    menu.add_category("Lighting");
    
    MenuItem smooth_light;
    smooth_light.label = "Smooth Lighting";
    smooth_light.type = MenuItemType::TOGGLE;
    smooth_light.bool_value = &Options::SMOOTH_LIGHTING;
    menu.add_item(smooth_light);
    
    MenuItem colored_light;
    colored_light.label = "Colored Lighting";
    colored_light.type = MenuItemType::TOGGLE;
    colored_light.bool_value = &Options::COLORED_LIGHTING;
    menu.add_item(colored_light);
    
    MenuItem fancy_trans;
    fancy_trans.label = "Fancy Translucency";
    fancy_trans.type = MenuItemType::TOGGLE;
    fancy_trans.bool_value = &Options::FANCY_TRANSLUCENCY;
    menu.add_item(fancy_trans);
    
    MenuItem light_steps;
    light_steps.label = "Light Propagation Steps";
    light_steps.type = MenuItemType::SLIDER;
    light_steps.int_value = &Options::LIGHT_STEPS_PER_TICK;
    light_steps.min_val = 256;
    light_steps.max_val = 8192;
    light_steps.step = 256;
    menu.add_item(light_steps);
    
    // ====== SHADOWS ======
    menu.add_category("Shadows");
    
    MenuItem shadows_en;
    shadows_en.label = "Enable Shadows";
    shadows_en.type = MenuItemType::TOGGLE;
    shadows_en.bool_value = &Options::SHADOWS_ENABLED;
    menu.add_item(shadows_en);
    
    MenuItem shadow_qual;
    shadow_qual.label = "Shadow Quality";
    shadow_qual.type = MenuItemType::SELECT;
    shadow_qual.options = {"512 (Low)", "1024 (Medium)", "2048 (High)", "4096 (Ultra)"};
    shadow_qual.selected_index = &SettingsStorage::shadow_quality_index;
    shadow_qual.option_values = {512, 1024, 2048, 4096};
    shadow_qual.int_value = &Options::SHADOW_MAP_RESOLUTION;
    menu.add_item(shadow_qual);
    
    MenuItem cascades;
    cascades.label = "Shadow Cascades";
    cascades.type = MenuItemType::SLIDER;
    cascades.int_value = &Options::SHADOW_CASCADES;
    cascades.min_val = 1;
    cascades.max_val = 4;
    cascades.step = 1;
    menu.add_item(cascades);
    
    MenuItem pcf;
    pcf.label = "Shadow Softness (PCF)";
    pcf.type = MenuItemType::SLIDER;
    pcf.int_value = &Options::SHADOW_PCF_RADIUS;
    pcf.min_val = 0;
    pcf.max_val = 3;
    pcf.step = 1;
    menu.add_item(pcf);
    
    MenuItem log_weight;
    log_weight.label = "Cascade Distribution";
    log_weight.type = MenuItemType::SLIDER;
    log_weight.float_value = &Options::SHADOW_LOG_WEIGHT;
    log_weight.min_val = 0.0f;
    log_weight.max_val = 1.0f;
    log_weight.step = 0.05f;
    menu.add_item(log_weight);
    
    // ====== PERFORMANCE ======
    menu.add_category("Performance");
    
    MenuItem chunk_updates;
    chunk_updates.label = "Chunk Updates/Frame";
    chunk_updates.type = MenuItemType::SLIDER;
    chunk_updates.int_value = &Options::CHUNK_UPDATES;
    chunk_updates.min_val = 1;
    chunk_updates.max_val = 16;
    chunk_updates.step = 1;
    menu.add_item(chunk_updates);
    
    MenuItem max_frames;
    max_frames.label = "Max CPU Ahead Frames";
    max_frames.type = MenuItemType::SLIDER;
    max_frames.int_value = &Options::MAX_CPU_AHEAD_FRAMES;
    max_frames.min_val = 1;
    max_frames.max_val = 6;
    max_frames.step = 1;
    menu.add_item(max_frames);
    
    MenuItem smooth_fps;
    smooth_fps.label = "Smooth FPS";
    smooth_fps.type = MenuItemType::TOGGLE;
    smooth_fps.bool_value = &Options::SMOOTH_FPS;
    menu.add_item(smooth_fps);
    
    MenuItem indirect;
    indirect.label = "Indirect Rendering";
    indirect.type = MenuItemType::TOGGLE;
    indirect.bool_value = &Options::INDIRECT_RENDERING;
    menu.add_item(indirect);
    
    MenuItem advanced_gl;
    advanced_gl.label = "Advanced OpenGL";
    advanced_gl.type = MenuItemType::TOGGLE;
    advanced_gl.bool_value = &Options::ADVANCED_OPENGL;
    menu.add_item(advanced_gl);
    
    // ====== AUDIO ======
    menu.add_category("Audio");
    
    MenuItem music_vol;
    music_vol.label = "Music Volume";
    music_vol.type = MenuItemType::SLIDER;
    music_vol.float_value = &SettingsStorage::audio_volume;
    music_vol.min_val = 0.0f;
    music_vol.max_val = 1.0f;
    music_vol.step = 0.05f;
    menu.add_item(music_vol);
}

} // namespace Menu
