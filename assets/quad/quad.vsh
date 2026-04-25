#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

out vec2 out_uv;

void main()
{
    out_uv = in_uv;
    gl_Position = proj * view * model * vec4(in_vert, 1.0);
}