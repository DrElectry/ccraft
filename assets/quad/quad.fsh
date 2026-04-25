#version 330 core

out vec4 fragColor;

in vec2 out_uv;

uniform sampler2D tex;

void main() {
    vec4 data = texture(tex, out_uv);
    fragColor = data;
}