#include "text_renderer.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath> // Для std::floor
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

const char* textVertexShader = R"(
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

const char* textFragmentShader = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec3 textColor;
void main() {
    float alpha = texture(text, TexCoords).r;
    if (alpha < 0.5) discard;
    color = vec4(textColor, 1.0);
}
)";

TextRenderer::TextRenderer(unsigned int width, unsigned int height)
: scrWidth(width), scrHeight(height) {
    initShader();
    initRenderData();
    SetScreenSize(width, height);
}

TextRenderer::~TextRenderer() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
}

void TextRenderer::SetScreenSize(unsigned int width, unsigned int height) {
    scrWidth = width; scrHeight = height;
    projection = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
}

void TextRenderer::initShader() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &textVertexShader, NULL);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &textFragmentShader, NULL);
    glCompileShader(fs);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void TextRenderer::Load(std::string fontPath, unsigned int fontSize) {
    Characters.clear();
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "ERROR::FREETYPE: Could not open font file " << fontPath << std::endl;
        return;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, buffer.data(), 0)) return;

    float scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++) {
        int width, height, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, 0, scale, c, &width, &height, &xoff, &yoff);

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        int advance;
        stbtt_GetCodepointHMetrics(&info, c, &advance, 0);

        Character character = {
            texture,
            glm::ivec2(width, height),
            // ИСПРАВЛЕНИЕ: -yoff. В библиотеке STB ось Y идет вниз, а в OpenGL вверх.
            // Инверсия нужна, чтобы координаты "смещения вверх" стали положительными.
            glm::ivec2(xoff, -yoff),
            (unsigned int)(advance * scale)
        };
        Characters.insert(std::pair<char, Character>(c, character));
        stbtt_FreeBitmap(bitmap, nullptr);
    }
}

void TextRenderer::initRenderData() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void TextRenderer::RenderText(std::string text, float x, float y, float scale, glm::vec3 color) {
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), color.x, color.y, color.z);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    // Округляем начальную позицию для pixel-perfect рендера
    x = std::floor(x);

    // Вычисляем базовую линию Y от верха экрана
    float startY = scrHeight - std::floor(y);

    for (std::string::const_iterator c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        // Расчет координат.
        // xpos - левый край
        float xpos = std::floor(x + ch.Bearing.x * scale);

        // ypos - ВЕРХНИЙ край глифа.
        // startY - линия, от которой мы отступаем.
        // Bearing.y - высота буквы от базовой линии вверх.
        // Вычитаем Bearing.y, потому что Y растет вверх, а мы "спускаемся" от верхней границы экрана (startY)
        float ypos = std::floor(startY - (24.0f - ch.Bearing.y) * scale);
        // (24.0f - эвристическая константа "Ascender" для выравнивания,
        // можно заменить на `ch.Bearing.y` и настроить `y` в main.cpp, но так надежнее для разных букв)

        // Упрощенная формула, которая хорошо работает для верхнего выравнивания (Top-Left origin logic):
        // Мы хотим, чтобы y переданный в функцию, был координатой верхнего края строки.
        // startY = H - y.
        // Верх глифа должен быть ниже startY на (Ascender - Bearing.y).
        // Пусть Ascender ~ Size.y.
        ypos = std::floor(startY - (ch.Size.y - ch.Bearing.y) * scale - ch.Bearing.y*scale);

        // Самый простой и рабочий вариант:
        // Считаем startY базовой линией строки.
        // Верх глифа = Baseline + BearingY.
        ypos = std::floor(startY + ch.Bearing.y * scale);
        // Но тогда `y` в аргументах должен быть координатой Baseline, а не верха.
        // В main.cpp мы передаем y=10 (верх). Baseline должен быть y=30.
        // Давайте скорректируем это здесь:

        float baseline = startY - 20.0f * scale; // Смещаем вниз на высоту шрифта
        float top = std::floor(baseline + ch.Bearing.y * scale);

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Строим квад от `top` вниз на `h`
        float vertices[6][4] = {
            { xpos,     top,       0.0f, 0.0f },
            { xpos,     top - h,   0.0f, 1.0f },
            { xpos + w, top - h,   1.0f, 1.0f },

            { xpos,     top,       0.0f, 0.0f },
            { xpos + w, top - h,   1.0f, 1.0f },
            { xpos + w, top,       1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += std::floor(ch.Advance * scale);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
