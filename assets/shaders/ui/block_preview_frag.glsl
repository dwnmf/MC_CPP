#version 330 core
in vec3 vUV;
in float vShade;

out vec4 FragColor;

uniform sampler2DArray u_TextureArraySampler;
uniform vec3 u_LightDir;
uniform float u_Brightness;

void main() {
    vec4 tex = texture(u_TextureArraySampler, vUV);
    if (tex.a < 0.5) discard;

    float shade = clamp(vShade, 0.25, 1.1);
    float light_term = clamp(dot(normalize(vec3(0.35, 1.0, 0.35)), normalize(-u_LightDir)) * 0.6 + 0.5, 0.25, 1.2);
    float intensity = shade * light_term * clamp(u_Brightness, 0.25, 1.3);

    FragColor = vec4(tex.rgb * intensity, tex.a);
}
