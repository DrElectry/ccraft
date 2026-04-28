#version 330 core

layout(location = 0) in vec2 in_vert;
layout(location = 1) in vec2 in_uv;

out vec2 out_uv;

void main() {
    out_uv = in_uv;
    gl_Position = vec4(in_vert, 0.0, 1.0);
}
