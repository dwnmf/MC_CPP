#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <map>
#include <string>

class TextRenderer {
public:
    TextRenderer(unsigned int width, unsigned int height);
    ~TextRenderer();

    void Load(std::string font, unsigned int fontSize);
    void RenderText(std::string text, float x, float y, float scale, glm::vec3 color);

    // Обновление матрицы проекции при ресайзе окна
    void SetScreenSize(unsigned int width, unsigned int height);

private:
    struct Character {
        unsigned int TextureID; // ID текстуры глифа
        glm::ivec2   Size;      // Размер глифа
        glm::ivec2   Bearing;   // Смещение от линии шрифта
        unsigned int Advance;   // Смещение до следующего глифа
    };

    std::map<char, Character> Characters;
    unsigned int VAO, VBO;
    unsigned int shaderProgram;
    glm::mat4 projection;
    unsigned int scrWidth, scrHeight;

    void initRenderData();
    void initShader();
};
