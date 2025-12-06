#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 u_Proj;
uniform vec2 u_Pos;
uniform vec2 u_Size;

out vec2 v_UV;
out vec2 v_LocalPos;
out vec2 v_Size;

void main() {
    v_UV = aPos;
    v_LocalPos = aPos * u_Size;
    v_Size = u_Size;
    vec2 pos = aPos * u_Size + u_Pos;
    gl_Position = u_Proj * vec4(pos, 0.0, 1.0);
}
