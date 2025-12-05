#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aUV;
layout (location = 2) in float aShade;

uniform mat4 u_Model;
uniform mat4 u_ViewProj;

out vec3 vUV;
out float vShade;

void main() {
    vUV = aUV;
    vShade = aShade;
    gl_Position = u_ViewProj * u_Model * vec4(aPos, 1.0);
}
