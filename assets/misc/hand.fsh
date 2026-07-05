#version 330 core

layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gViewPosition;
layout(location = 3) out vec2 gRoughness;
layout(location = 4) out vec3 gBrightness;

in vec2 out_uv;
in vec3 out_normal;
in vec3 out_view_pos;
in vec3 out_tangent;
in vec3 out_bitangent;
in float out_light;

uniform sampler2D tex;
uniform sampler2D roug;
uniform sampler2D bright;
uniform sampler2D normal;
uniform uint id;

void main()
{
	const float tiles_per_row = 16.0;
	const float tile_size = 1.0 / tiles_per_row;

	uint tile_x = id % 16u;
	uint tile_y = id / 16u;

	vec2 tile_offset = vec2(float(tile_x) * tile_size, float(tile_y) * tile_size);
	vec2 atlas_uv = tile_offset + out_uv;

	vec4 data = texture(tex, atlas_uv);
	if (data.a < 0.1)
	{
		discard;
	}

	if (id == 18u)
	{
		data.rgb *= 4.0;
	}

	float lit = 0.12 + 0.88 * clamp(out_light, 0.0, 1.0);
	gAlbedo = data.rgb * lit;

	vec3 N = normalize(out_normal);
	vec3 T = normalize(out_tangent);
	vec3 B = normalize(out_bitangent);
	mat3 TBN = mat3(T, B, N);

	vec3 nSample = texture(normal, atlas_uv).rgb;
	vec3 tangentNorm = normalize(nSample * 2.0 - 1.0);

	gNormal = normalize(TBN * tangentNorm);
	gRoughness = texture(roug, atlas_uv).bb;
	gBrightness = texture(bright, atlas_uv).rgb;
	gViewPosition = vec4(out_view_pos, 1.0);
}