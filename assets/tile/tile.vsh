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
out vec3 out_tangent;
out vec3 out_bitangent;
out vec3 out_view_pos;
out float out_light;

void main()
{
    out_uv = in_uv;
    out_light = in_light;

    vec4 world_pos = model * vec4(in_vert, 1.0);
    vec4 view_pos = view * world_pos;

    out_view_pos = view_pos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * in_normal);
    out_normal = N;

    vec3 T = normalize(in_tangent);
    vec3 B = normalize(in_bitangent);

    out_tangent = T;
    out_bitangent = B;

    gl_Position = proj * view_pos;
}
