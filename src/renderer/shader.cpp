#include "shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace {
bool check_compile(GLuint shader, const std::string& name) {
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        std::string log(log_len, '\0');
        glGetShaderInfoLog(shader, log_len, nullptr, log.data());
        std::cout << "ERROR::SHADER::COMPILATION_FAILED (" << name << "): " << log << std::endl;
        return false;
    }
    return true;
}

bool check_link(GLuint program) {
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        std::string log(log_len, '\0');
        glGetProgramInfoLog(program, log_len, nullptr, log.data());
        std::cout << "ERROR::SHADER::LINK_FAILED: " << log << std::endl;
        return false;
    }
    return true;
}
}

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    std::string vertexCode, fragmentCode;
    std::ifstream vShaderFile, fShaderFile;
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    } catch (std::ifstream::failure& e) {
        std::cout << "ERROR::SHADER::FILE_NOT_READ: " << e.what() << std::endl;
        ID = 0;
        return;
    }
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();
    unsigned int vertex, fragment;

    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    if (!check_compile(vertex, vertexPath)) { ID = 0; glDeleteShader(vertex); return; }

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    if (!check_compile(fragment, fragmentPath)) { ID = 0; glDeleteShader(vertex); glDeleteShader(fragment); return; }

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    bool linked = check_link(ID);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (!linked) {
        glDeleteProgram(ID);
        ID = 0;
    }
}

Shader::~Shader() { if (ID) glDeleteProgram(ID); }
void Shader::use() { if (ID) glUseProgram(ID); }
int Shader::find_uniform(const std::string& name) { return glGetUniformLocation(ID, name.c_str()); }
void Shader::setMat4(int loc, const glm::mat4& mat) const { glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mat)); }
void Shader::setInt(int loc, int val) const { glUniform1i(loc, val); }
void Shader::setFloat(int loc, float val) const { glUniform1f(loc, val); }
void Shader::setVec2i(int loc, int x, int y) const { glUniform2i(loc, x, y); }
void Shader::setVec2(int loc, const glm::vec2& vec) const { glUniform2fv(loc, 1, glm::value_ptr(vec)); }
void Shader::setVec3(int loc, const glm::vec3& vec) const { glUniform3fv(loc, 1, glm::value_ptr(vec)); }
void Shader::setMat4Array(int loc, const std::vector<glm::mat4>& mats) const {
    if (loc < 0 || mats.empty()) return;
    glUniformMatrix4fv(loc, static_cast<GLsizei>(mats.size()), GL_FALSE, glm::value_ptr(mats[0]));
}
void Shader::setFloatArray(int loc, const std::vector<float>& values) const {
    if (loc < 0 || values.empty()) return;
    glUniform1fv(loc, static_cast<GLsizei>(values.size()), values.data());
}
