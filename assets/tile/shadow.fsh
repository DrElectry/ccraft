#version 330 core

in vec2 out_uv;
in vec3 out_normal;
in vec3 out_view_pos;
in float out_light;

uniform sampler2D tex;

void main() {
    vec4 data = texture(tex, out_uv);
    if (data.a < 0.1) {
        discard;
    }
}
