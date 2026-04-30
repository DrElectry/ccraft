#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform mat4 prev_view_proj;
uniform vec2 screen_size;

out vec2 out_uv;
out vec3 out_normal;
out vec2 out_velocity;

void main()
{
    out_normal = in_normal;
    out_uv = in_uv;

    vec4 current_pos = proj * view * model * vec4(in_vert, 1.0);
    vec4 prev_pos = prev_view_proj * model * vec4(in_vert, 1.0);

    vec2 current_ndc = current_pos.xy / current_pos.w;
    vec2 prev_ndc = prev_pos.xy / prev_pos.w;

    vec2 velocity = current_ndc - prev_ndc;
    out_velocity = velocity * screen_size;

    gl_Position = current_pos;
}
