#version 330 core
out vec4 FragColor;

in vec2 v_UV;
in vec2 v_LocalPos;
in vec2 v_Size;

// Colors
uniform vec4 u_ColorTop;
uniform vec4 u_ColorBottom;

// Border
uniform vec4 u_BorderColor;
uniform float u_BorderWidth;

// Rounded corners
uniform float u_CornerRadius;

// Glow effect
uniform vec4 u_GlowColor;
uniform float u_GlowIntensity;

// Time for animations
uniform float u_Time;

// Animation flags
uniform int u_PulseEnabled;
uniform int u_ShineEnabled;

// Helper function for rounded rectangle SDF
float roundedRectSDF(vec2 pos, vec2 halfSize, float radius) {
    vec2 q = abs(pos) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main() {
    vec2 center = v_Size * 0.5;
    vec2 pos = v_LocalPos - center;
    vec2 halfSize = v_Size * 0.5;
    
    float radius = min(u_CornerRadius, min(halfSize.x, halfSize.y));
    
    // Distance to rounded rectangle edge
    float dist = roundedRectSDF(pos, halfSize, radius);
    
    // Anti-aliased edge
    float aa = fwidth(dist) * 1.5;
    float alpha = 1.0 - smoothstep(-aa, aa, dist);
    
    if (alpha < 0.001) {
        discard;
    }
    
    // Gradient from top to bottom
    vec4 baseColor = mix(u_ColorTop, u_ColorBottom, v_UV.y);
    
    // Border
    float innerDist = dist + u_BorderWidth;
    float borderFactor = 1.0 - smoothstep(-aa, aa, innerDist);
    
    // Pulse animation for selected slots
    float pulse = 1.0;
    if (u_PulseEnabled != 0) {
        pulse = 0.85 + 0.15 * sin(u_Time * 3.0);
    }
    
    // Shine effect (moving highlight)
    float shine = 0.0;
    if (u_ShineEnabled != 0) {
        float shinePos = mod(u_Time * 0.5, 2.0) - 0.5;
        float diag = (v_UV.x + v_UV.y) * 0.5;
        shine = smoothstep(shinePos - 0.1, shinePos, diag) * 
                smoothstep(shinePos + 0.3, shinePos + 0.1, diag) * 0.3;
    }
    
    // Glow on the edge
    float glowDist = abs(dist);
    float glow = exp(-glowDist * 0.2) * u_GlowIntensity;
    
    // Mix border and fill
    vec4 fillColor = baseColor;
    fillColor.rgb += shine; // Add shine
    fillColor.rgb *= pulse; // Apply pulse
    
    vec4 finalColor = mix(fillColor, u_BorderColor, borderFactor);
    
    // Add glow
    finalColor.rgb += u_GlowColor.rgb * glow * u_GlowColor.a;
    
    finalColor.a *= alpha;
    
    FragColor = finalColor;
}
