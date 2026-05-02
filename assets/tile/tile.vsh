#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

out vec2 out_uv;
out vec3 out_normal;
out vec3 out_view_pos;

void main()
{
    out_normal = in_normal;
    out_uv = in_uv;

    vec4 world_pos = model * vec4(in_vert, 1.0);
    vec4 view_pos = view * world_pos;
    out_view_pos = view_pos.xyz;

    gl_Position = proj * view_pos;
}