#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_light;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

out vec2 out_uv;
out vec3 out_normal;
out vec3 out_view_pos;
out vec3 out_tangent;
out vec3 out_bitangent;
out float out_light;

void main()
{
	out_uv = in_uv;
	out_light = in_light;

	vec4 world_pos = model * vec4(in_vert, 1.0);
	vec4 view_pos  = view * world_pos;
	out_view_pos = view_pos.xyz;

	mat3 normal_matrix = mat3(transpose(inverse(model)));
	vec3 world_normal = normalize(normal_matrix * in_normal);
	vec3 world_tangent = normalize(normal_matrix * in_tangent);
	vec3 world_bitangent = normalize(normal_matrix * in_bitangent);

	vec3 N_view = normalize(mat3(view) * world_normal);
	vec3 T_view = normalize(mat3(view) * world_tangent);
	vec3 B_view = normalize(mat3(view) * world_bitangent);

	T_view = normalize(T_view - dot(T_view, N_view) * N_view);
	B_view = normalize(cross(N_view, T_view));

	out_normal = N_view;
	out_tangent = T_view;
	out_bitangent = B_view;

	gl_Position = proj * view_pos;
}