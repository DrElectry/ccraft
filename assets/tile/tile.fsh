#version 330 core

layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gViewPosition;
layout(location = 3) out vec2 gRoughness;

in vec2 out_uv;
in vec3 out_normal;
in vec3 out_tangent;
in vec3 out_bitangent;
in vec3 out_view_pos;
in float out_light;

uniform sampler2D tex;
uniform sampler2D roug;
uniform sampler2D normal;

void main()
{
    vec4 data = texture(tex, out_uv);
    if (data.a < 0.1)
        discard;

    float lit = 0.12 + 0.88 * clamp(out_light, 0.0, 1.0);
    gAlbedo = data.rgb * lit;

    vec3 n = texture(normal, out_uv).rgb;
    vec3 tangentNormal = normalize(n * 2.0 - 1.0);

    mat3 TBN = mat3(
        normalize(out_tangent),
        normalize(out_bitangent),
        normalize(out_normal)
    );

    gNormal = normalize(TBN * tangentNormal);

    gRoughness = texture(roug, out_uv).bb;
    gViewPosition = vec4(out_view_pos, 1.0);
}