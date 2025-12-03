#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform bool u_IsUnderwater;
uniform float u_Time;

void main() {
    vec2 uv = TexCoords;

    if (u_IsUnderwater) {
        float waveStrength = 0.01;
        uv.x += sin(uv.y * 10.0 + u_Time * 2.0) * waveStrength;
        uv.y += cos(uv.x * 10.0 + u_Time * 2.0) * waveStrength;

        float r = texture(screenTexture, uv + vec2(0.003, 0.0)).r;
        float g = texture(screenTexture, uv).g;
        float b = texture(screenTexture, uv - vec2(0.003, 0.0)).b;
        vec3 col = vec3(r, g, b);

        vec3 waterColor = vec3(0.2, 0.4, 0.9);
        col = col * waterColor;

        float dist = distance(uv, vec2(0.5));
        col *= smoothstep(0.8, 0.2, dist * 0.8);

        FragColor = vec4(col, 1.0);
    } else {
        FragColor = texture(screenTexture, TexCoords);
    }
}
