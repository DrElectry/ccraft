#version 330 core

layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec2 gVelocity;
layout(location = 3) out vec4 gViewPosition;

in vec2 out_uv;
in vec3 out_normal;
in vec2 out_velocity;
in vec3 out_view_pos;

uniform sampler2D tex;

void main() {
    vec4 data = texture(tex, out_uv);
    if (data.a < 0.1) {
        discard;
    }
    gAlbedo = data.rgb;
    gNormal = out_normal;
    gVelocity = out_velocity;
    gViewPosition = vec4(out_view_pos, 1.0);
}
