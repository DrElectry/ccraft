#version 330 core

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_uv;

out vec2 out_uv;

uniform mat4 model;
uniform mat4 projection;
uniform float flip_v;

void main() {
    gl_Position = projection * model * vec4(in_pos, 0.0, 1.0);
    out_uv = vec2(in_uv.x, mix(in_uv.y, 1.0 - in_uv.y, flip_v));
}
