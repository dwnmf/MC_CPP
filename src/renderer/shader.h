#pragma once
#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>
#include <vector>

class Shader {
public:
    unsigned int ID;
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();
    bool valid() const { return ID != 0; }
    void use();
    int find_uniform(const std::string& name);
    void setMat4(int location, const glm::mat4& mat) const;
    void setInt(int location, int value) const;
    void setFloat(int location, float value) const;
    void setVec2i(int location, int x, int y) const;
    void setVec2(int location, const glm::vec2& value) const;
    void setVec3(int location, const glm::vec3& value) const;
    void setMat4Array(int location, const std::vector<glm::mat4>& mats) const;
    void setFloatArray(int location, const std::vector<float>& values) const;
};
