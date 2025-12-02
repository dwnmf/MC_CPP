#pragma once
#include <vector>
#include <string>
#include <glad/glad.h>

class TextureManager {
public:
    int texture_width, texture_height, max_textures;
    std::vector<std::string> textures;
    GLuint texture_array;

    TextureManager(int w, int h, int max_tex);
    void generate_mipmaps();
    void add_texture(std::string texture);
    int get_texture_index(std::string texture);
};
