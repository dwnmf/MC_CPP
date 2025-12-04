#version 330 core

in vec3 v_TexCoords;
uniform sampler2DArray u_TextureArraySampler;

void main(void) {
    // Alpha test for foliage/transparent textures
    vec4 tex = texture(u_TextureArraySampler, v_TexCoords);
    if (tex.a < 0.5) discard;
}
