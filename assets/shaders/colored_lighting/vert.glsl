#version 330

#define CHUNK_WIDTH 16
#define CHUNK_LENGTH 16

uniform ivec2 u_ChunkPosition;
uniform mat4 u_MVPMatrix;
uniform float u_Daylight;

layout(location = 0) in uint a_Data0;
layout(location = 1) in uint a_Data1;
layout(location = 2) in uint a_Data2;

out vec3 v_Position;
out vec3 v_TexCoords;
out vec3 v_Light;

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

	float blocklightMultiplier = pow(0.8, 15.0 - a_Light);
	float skylightMultiplier = (a_Skylight > 0.0)
		? pow(0.8, 15.0 - a_Skylight)
		: 0.0;
	float skyLight = skylightMultiplier * u_Daylight;

	v_Light = vec3(
		clamp(blocklightMultiplier * 1.5, skyLight, 1.0),
		clamp(blocklightMultiplier * 1.25, skyLight, 1.0),
		clamp(skylightMultiplier * (2.0 - pow(u_Daylight, 2)), blocklightMultiplier, 1.0)
	) * shading;

	gl_Position = u_MVPMatrix * vec4(v_Position, 1.0);
}
