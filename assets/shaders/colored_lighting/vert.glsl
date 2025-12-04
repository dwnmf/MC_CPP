#version 330

#define CHUNK_WIDTH 16
#define CHUNK_LENGTH 16

uniform ivec2 u_ChunkPosition;
uniform mat4 u_MVPMatrix;
uniform mat4 u_ViewMatrix;
uniform float u_Daylight;

layout(location = 0) in uint a_Data0;
layout(location = 1) in uint a_Data1;
layout(location = 2) in uint a_Data2;

out vec3 v_Position;
out vec3 v_TexCoords;
out vec3 v_Light;
out float v_ViewDepth;

int decode_signed16(uint v) {
	return int(int(v << 16) >> 16);
}

void main(void) {
	int px = decode_signed16(a_Data0);
	int py = decode_signed16(a_Data0 >> 16);
	int pz = decode_signed16(a_Data1);

	float u = float((a_Data1 >> 16) & 0xFFu) / 255.0;
	float v = float((a_Data1 >> 24) & 0xFFu) / 255.0;

	float layer = float(a_Data2 & 0xFFu);
	float shading = float((a_Data2 >> 8) & 0xFFu) / 255.0;
	float a_Light = float((a_Data2 >> 16) & 0xFu);
	float a_Skylight = float((a_Data2 >> 20) & 0xFu);

	vec3 a_LocalPosition = vec3(px, py, pz) / 16.0;

	v_Position = vec3(u_ChunkPosition.x * CHUNK_WIDTH + a_LocalPosition.x,
						a_LocalPosition.y,
						u_ChunkPosition.y * CHUNK_LENGTH + a_LocalPosition.z);
	v_TexCoords = vec3(u, v, layer);

	// --- Colored lighting model ---
	float blockLevel = a_Light / 15.0;
	float skyLevel = a_Skylight / 15.0;

	vec3 blockTint = vec3(1.10, 0.85, 0.65);
	vec3 skyNight = vec3(0.25, 0.35, 0.55);
	vec3 skyDay = vec3(0.85, 0.95, 1.05);
	vec3 skyTint = mix(skyNight, skyDay, clamp(u_Daylight, 0.0, 1.0));

	float blockIntensity = pow(blockLevel, 1.1);
	float skyIntensity = pow(skyLevel * u_Daylight, 1.2);

	vec3 light = blockTint * blockIntensity + skyTint * skyIntensity;
	light = max(light, vec3(0.02));
	light = min(light, vec3(1.0));

	v_Light = light * shading;

	vec4 viewPos = u_ViewMatrix * vec4(v_Position, 1.0);
	v_ViewDepth = -viewPos.z;

	gl_Position = u_MVPMatrix * vec4(v_Position, 1.0);
}
