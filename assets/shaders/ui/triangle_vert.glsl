#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform vec2 u_Offset;
uniform vec2 u_Scale;

void main() {
    vec2 pos = aPos * u_Scale + u_Offset;
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoords = aTexCoords;
}
