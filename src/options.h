#pragma once
#include <glad/glad.h>

namespace Options {
    inline int RENDER_DISTANCE = 8;
    inline float FOV = 90.0f;
    inline bool INDIRECT_RENDERING = false;
    inline bool ADVANCED_OPENGL = false;
    inline int CHUNK_UPDATES = 4;
    inline bool VSYNC = false;
    inline int MAX_CPU_AHEAD_FRAMES = 3;
    inline bool SMOOTH_FPS = false;
    inline bool SMOOTH_LIGHTING = true;
    inline bool FANCY_TRANSLUCENCY = true;
    inline int MIPMAP_TYPE = GL_NEAREST_MIPMAP_LINEAR;
    inline bool COLORED_LIGHTING = true;
    inline int ANTIALIASING = 0;
    inline int LIGHT_STEPS_PER_TICK = 2048;
}
