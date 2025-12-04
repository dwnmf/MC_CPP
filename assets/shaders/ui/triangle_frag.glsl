#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform float u_Fullness;
uniform float u_Time;

// Pseudo-noise for smoky flicker
float noise(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = TexCoords;
    float x = uv.x - 0.5;
    float y = 1.0 - uv.y; // invert vertically so the triangle points down

    float halfWidth = 0.5 * (1.0 - y);
    float distX = abs(x);
    bool inside = (distX < halfWidth) && (y >= 0.0) && (y <= 1.0);
    if (!inside) discard;

    float distToEdge = halfWidth - distX;
    float distToTop = y;
    float borderDist = min(distToEdge, distToTop);

    float borderThickness = 0.08;

    float glowAnim = 0.6 + 0.4 * sin(u_Time * 3.0);
    float flicker = 0.7 + 0.3 * noise(uv * 6.0 + u_Time * 0.5);

    vec3 blackBody = vec3(0.0);
    vec3 redGlow = vec3(1.0, 0.0, 0.0) * 1.5;

    float mixFactor = smoothstep(borderThickness, 0.0, borderDist);
    vec3 finalColor = mix(blackBody, redGlow, mixFactor * glowAnim * flicker);

    float alpha = 1.0;
    if (u_Fullness < 0.5) {
        finalColor = vec3(0.2, 0.0, 0.0);
        alpha = 0.3;
    }

    FragColor = vec4(finalColor, alpha);
}
