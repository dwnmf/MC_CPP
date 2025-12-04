#include "texture_manager.h"
#include "../options.h"
#include <iostream>
#include <algorithm>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

TextureManager::TextureManager(int w, int h, int max_tex) : texture_width(w), texture_height(h), max_textures(max_tex) {
#ifdef UNIT_TEST
    texture_array = 0;
    return;
#endif
    glGenTextures(1, &texture_array);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, Options::MIPMAP_TYPE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Исправление перевернутых текстур
    stbi_set_flip_vertically_on_load(true);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, texture_width, texture_height, max_textures, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}

void TextureManager::generate_mipmaps() {
#ifndef UNIT_TEST
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
#endif
}

void TextureManager::add_texture(std::string texture) {
    if (std::find(textures.begin(), textures.end(), texture) != textures.end()) return;
    textures.push_back(texture);
    int index = textures.size() - 1;
#ifdef UNIT_TEST
    (void)index;
    return;
#endif
    std::string path = "assets/textures/" + texture + ".png";
    int width, height, nrChannels;
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
    if (data) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);
        std::cout << "Loading texture " << path << " (" << width << "x" << height << ", channels=" << nrChannels << ") into layer " << index << std::endl;
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    } else {
        std::cout << "Failed load tex: " << path << std::endl;
    }
}

int TextureManager::get_texture_index(std::string texture) {
    auto it = std::find(textures.begin(), textures.end(), texture);
    return (it != textures.end()) ? std::distance(textures.begin(), it) : 0;
}
