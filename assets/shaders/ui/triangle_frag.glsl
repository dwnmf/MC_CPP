#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform float u_Fullness;
uniform float u_Time;

// Simple hash-based noise
float noise(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = TexCoords;

    // Local triangle space: inverted black triangle pointing down
    float x = uv.x - 0.5;
    float y = 1.0 - uv.y;

    float halfWidth = 0.5 * (1.0 - y);
    float distX = abs(x);
    bool inside = (distX < halfWidth) && (y >= 0.0) && (y <= 1.0);
    if (!inside) discard;

    float distToEdge = halfWidth - distX;
    float distToTop  = y;
    float borderDist = min(distToEdge, distToTop);

    float fullness  = clamp(u_Fullness, 0.0, 1.0);
    float emptiness = 1.0 - fullness;

    // --- Base body: dark inverted triangle ---
    vec3 baseColor = mix(vec3(0.05, 0.0, 0.0), vec3(0.12, 0.0, 0.0), fullness);
    float baseAlpha = mix(0.4, 1.0, fullness);

    // --- Unstable glow around edges (not perfectly glued to contour) ---
    float edgeJitter  = (noise(uv * 12.0 + vec2(u_Time * 3.1, -u_Time * 1.7)) - 0.5) * 0.06;
    float thickNoise  = 0.04 + 0.06 * noise(uv * 4.0 + u_Time * 1.3);
    float glowMask    = smoothstep(thickNoise, 0.0, borderDist + edgeJitter);

    float glowPulse   = 0.7 + 0.3 * sin(u_Time * 5.0 + uv.x * 10.0);
    float glowNoise   = 0.7 + 0.3 * noise(uv * 10.0 + vec2(u_Time * 0.8, -u_Time * 0.6));
    float healthBoost = clamp(0.3 + emptiness * 0.7, 0.3, 1.0);

    vec3 glowColor = vec3(1.0, 0.25, 0.0) * healthBoost;
    vec3 glow = glowColor * glowMask * glowPulse * glowNoise;

    // --- Always-on unstable "smoke" inside triangle ---
    vec2 smokeUv = uv;
    smokeUv.y += u_Time * 0.18;
    smokeUv.x += sin(u_Time * 0.7) * 0.05;

    float smokeBase   = noise(smokeUv * 3.5);
    float smokeDetail = noise(smokeUv * 9.0 + 13.17);
    float smokeField  = smoothstep(0.45, 0.85, smokeBase * 0.7 + smokeDetail * 0.3);

    // Prefer smoke toward the tip and center so it looks like rising wisps
    float centerFalloff = 1.0 - smoothstep(0.0, 0.6, abs(x));
    float heightMask    = smoothstep(0.2, 1.0, y);
    float smokeMask     = smokeField * centerFalloff * heightMask;

    vec3 smokeColor = mix(vec3(0.10, 0.10, 0.10), vec3(0.45, 0.18, 0.18), healthBoost);
    vec3 smoke = smokeColor * smokeMask * 0.7;

    // Combine
    vec3 color = baseColor + glow + smoke;
    color = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, baseAlpha);
}
