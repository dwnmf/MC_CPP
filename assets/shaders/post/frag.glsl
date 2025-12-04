#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform bool u_IsUnderwater;
uniform float u_Time;
uniform vec2 u_ScreenSize;
uniform float u_SSAOStrength;

// Lightweight depth-based SSAO (no normals)
float calcSSAO(vec2 uv) {
    float center = texture(depthTexture, uv).r;
    // If depth buffer is empty (sky), skip occlusion
    if (center >= 0.999) return 1.0;

    vec2 texel = 1.0 / u_ScreenSize;
    float radius = 3.0;
    float occlusion = 0.0;
    float samples = 0.0;

    // 8-tap kernel
    vec2 offsets[8] = vec2[](
        vec2(-1, -1), vec2(0, -1), vec2(1, -1),
        vec2(-1,  0),              vec2(1,  0),
        vec2(-1,  1), vec2(0,  1), vec2(1,  1)
    );

    for (int i = 0; i < 8; i++) {
        vec2 sampleUv = uv + offsets[i] * texel * radius;
        float sampleDepth = texture(depthTexture, sampleUv).r;
        // Bias to avoid self-occlusion
        float diff = center - sampleDepth - 0.001;
        occlusion += clamp(diff * 8.0, 0.0, 1.0);
        samples += 1.0;
    }

    float ao = 1.0 - (occlusion / samples) * u_SSAOStrength;
    return clamp(ao, 0.4, 1.0);
}

void main() {
    vec2 uv = TexCoords;
    vec3 col;

    if (u_IsUnderwater) {
        float waveStrength = 0.01;
        uv.x += sin(uv.y * 10.0 + u_Time * 2.0) * waveStrength;
        uv.y += cos(uv.x * 10.0 + u_Time * 2.0) * waveStrength;

        float r = texture(screenTexture, uv + vec2(0.003, 0.0)).r;
        float g = texture(screenTexture, uv).g;
        float b = texture(screenTexture, uv - vec2(0.003, 0.0)).b;
        col = vec3(r, g, b);

        vec3 waterColor = vec3(0.2, 0.4, 0.9);
        col = col * waterColor;

        float dist = distance(uv, vec2(0.5));
        col *= smoothstep(0.8, 0.2, dist * 0.8);
    } else {
        col = texture(screenTexture, TexCoords).rgb;
    }

    float ao = calcSSAO(TexCoords);
    col *= ao;

    FragColor = vec4(col, 1.0);
}
