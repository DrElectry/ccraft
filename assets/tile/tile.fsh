#version 330 core

layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gViewPosition;
layout(location = 3) out vec2 gRoughness;

in vec2 out_uv;
in vec3 out_normal;
in vec3 out_view_pos;
in float out_light;

uniform sampler2D tex;
uniform sampler2D roug;

void main() {
    vec4 data = texture(tex, out_uv);
    if (data.a < 0.1) {
        discard;
    }

    float lit = 0.12 + 0.88 * clamp(out_light, 0.0, 1.0);
    gAlbedo = data.rgb * lit;
    gNormal = out_normal;
    gRoughness = texture(roug, out_uv).bb;
    gViewPosition = vec4(out_view_pos, 1.0);
}