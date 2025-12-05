#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 u_Proj;
uniform vec2 u_Pos;
uniform vec2 u_Size;

void main() {
    vec2 pos = aPos * u_Size + u_Pos;
    gl_Position = u_Proj * vec4(pos, 0.0, 1.0);
}
