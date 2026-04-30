#version 330 core

layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec2 gVelocity;

in vec2 out_uv;
in vec3 out_normal;
in vec2 out_velocity;

uniform sampler2D tex;

void main() {
    vec4 data = texture(tex, out_uv);
    if (data.a < 0.1) {
        discard;
    }
    gAlbedo = data.rgb;
    gNormal = out_normal;
    gVelocity = out_velocity;
}
