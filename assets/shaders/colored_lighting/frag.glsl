#version 330

out vec4 fragColor;

uniform sampler2DArray u_TextureArraySampler;

#define MAX_CASCADES 4

uniform sampler2DArray u_ShadowMap;
uniform mat4 u_LightSpaceMatrices[MAX_CASCADES];
uniform float u_CascadeSplits[MAX_CASCADES];
uniform int u_ShadowCascadeCount;
uniform vec2 u_ShadowTexelSize;
uniform float u_ShadowMinBias;
uniform float u_ShadowSlopeBias; // reserved, not used yet
uniform int u_ShadowPCFRadius;

in vec3 v_Position;
in vec3 v_TexCoords;
in vec3 v_Light;
in float v_ViewDepth;

float calculateShadowFactor(vec3 worldPos, float viewDepth) {
	if (u_ShadowCascadeCount <= 0) return 1.0;

	int cascadeIndex = u_ShadowCascadeCount - 1;
	for (int i = 0; i < u_ShadowCascadeCount; ++i) {
		if (viewDepth < u_CascadeSplits[i]) {
			cascadeIndex = i;
			break;
		}
	}

	vec4 lightClip = u_LightSpaceMatrices[cascadeIndex] * vec4(worldPos, 1.0);
	vec3 lightND = lightClip.xyz / lightClip.w;
	vec3 lightUVZ = lightND * 0.5 + 0.5;

	// Outside of shadow map
	if (lightUVZ.x < 0.0 || lightUVZ.x > 1.0 ||
	    lightUVZ.y < 0.0 || lightUVZ.y > 1.0 ||
	    lightUVZ.z < 0.0 || lightUVZ.z > 1.0) {
		return 1.0;
	}

	int radius = u_ShadowPCFRadius;
	if (radius <= 0) {
		float closestDepth = texture(u_ShadowMap, vec3(lightUVZ.xy, cascadeIndex)).r;
		float currentDepth = lightUVZ.z - u_ShadowMinBias;
		return currentDepth <= closestDepth ? 1.0 : 0.5; // немного амбиента
	}

	float samples = 0.0;
	float visible = 0.0;
	for (int x = -radius; x <= radius; ++x) {
		for (int y = -radius; y <= radius; ++y) {
			vec2 offset = vec2(x, y) * u_ShadowTexelSize;
			float closestDepth = texture(u_ShadowMap, vec3(lightUVZ.xy + offset, cascadeIndex)).r;
			float currentDepth = lightUVZ.z - u_ShadowMinBias;
			if (currentDepth <= closestDepth) visible += 1.0;
			samples += 1.0;
		}
	}
	if (samples <= 0.0) return 1.0;

	float visibility = visible / samples;
	visibility = clamp(visibility, 0.0, 1.0);
	// Очень мягкие тени для избежания полной черноты:
	// 0.0 -> 0.9, 1.0 -> 1.0  (диапазон [0.9, 1.0])
	return mix(0.9, 1.0, visibility);
}


void main(void) {
	vec4 textureColor = texture(u_TextureArraySampler, v_TexCoords);

	// Прозрачные блоки (листва, стекло и т.п.) — как раньше
	if (textureColor.a <= 0.5) {
		discard;
	}

	float shadowFactor = calculateShadowFactor(v_Position, v_ViewDepth);
	fragColor = textureColor * vec4(v_Light * shadowFactor, 1.0);
}
