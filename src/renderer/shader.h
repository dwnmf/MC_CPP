#pragma once
#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    unsigned int ID;
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();
    void use();
    int find_uniform(const std::string& name);
    void setMat4(int location, const glm::mat4& mat) const;
    void setInt(int location, int value) const;
    void setFloat(int location, float value) const;
    void setVec2i(int location, int x, int y) const;
};
