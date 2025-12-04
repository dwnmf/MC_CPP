#version 330 core

#define CHUNK_WIDTH 16
#define CHUNK_LENGTH 16

uniform ivec2 u_ChunkPosition;
uniform mat4 u_LightSpaceMatrix;

layout(location = 0) in uint a_Data0;
layout(location = 1) in uint a_Data1;
layout(location = 2) in uint a_Data2;

out vec3 v_TexCoords;

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

	vec3 local_pos = vec3(px, py, pz) / 16.0;
	vec3 world_pos = vec3(u_ChunkPosition.x * CHUNK_WIDTH + local_pos.x,
						local_pos.y,
						u_ChunkPosition.y * CHUNK_LENGTH + local_pos.z);

	v_TexCoords = vec3(u, v, layer);
	gl_Position = u_LightSpaceMatrix * vec4(world_pos, 1.0);
}
