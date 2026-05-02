#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform mat4 prev_view_proj;
uniform vec2 screen_size;
uniform float time;

out vec2 out_uv;
out vec3 out_normal;
out vec2 out_velocity;
out vec3 out_view_pos;
out vec3 out_pos;

void main()
{
    out_normal = in_normal;
    out_uv = in_uv;

    vec3 pos = in_vert;
    vec3 world_pos = (model * vec4(in_vert, 1.0)).xyz;

    float tile_size = 1.0/16.0;

    if (mod(time, 1.0) < 0.1) {
        out_uv.x+=tile_size;
    }

    float wave = sin(world_pos.x * 2.0 + world_pos.z * 2.0 + time*8.0) * 0.01;
    pos.y += wave;

    pos.y -= 0.1;

    out_pos = pos;

    vec4 current_pos = proj * view * model * vec4(pos, 1.0);
    vec4 prev_pos = prev_view_proj * model * vec4(pos, 1.0);

    vec4 view_pos = view * model * vec4(pos, 1.0);
    out_view_pos = view_pos.xyz;

    vec2 current_ndc = current_pos.xy / current_pos.w;
    vec2 prev_ndc = prev_pos.xy / prev_pos.w;

    vec2 velocity = current_ndc - prev_ndc;
    out_velocity = velocity * screen_size;

    gl_Position = current_pos;
}