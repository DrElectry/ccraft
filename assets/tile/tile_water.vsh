#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform float time;

out vec2 out_uv;
out vec3 out_normal;
out vec3 out_view_pos;
out vec3 out_pos;

float wave(vec3 p)
{
    return sin(p.x * 2.0 + p.z * 2.0 + time * 8.0) * 0.01;
}

void main()
{
    out_uv = in_uv;

    vec3 world_pos = (model * vec4(in_vert, 1.0)).xyz;

    float h = wave(world_pos);
    float eps = 0.001;

    float hx = wave(world_pos + vec3(eps, 0.0, 0.0));
    float hz = wave(world_pos + vec3(0.0, 0.0, eps));

    float wave_height = h - 0.1;

    vec3 displaced = world_pos;
    displaced.y += wave_height;

    vec3 dx = vec3(eps, hx - h, 0.0);
    vec3 dz = vec3(0.0, hz - h, eps);

    vec3 normal_world = normalize(cross(dz, dx));

    out_normal = normal_world;

    vec4 view_pos = view * vec4(displaced, 1.0);
    out_view_pos = view_pos.xyz;

    out_pos = displaced;

    gl_Position = proj * view_pos;
}