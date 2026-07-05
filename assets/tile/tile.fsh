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
in vec3 TangentViewDir;

uniform sampler2D tex;
uniform sampler2D roug;
uniform sampler2D normal;

void main()
{
    vec3 T = normalize(out_tangent);
    vec3 B = normalize(out_bitangent);
    vec3 N = normalize(out_normal);
    mat3 TBN = mat3(T, B, N);

    vec2 texCoords = out_uv;

    vec4 albedo = texture(tex, texCoords);
    if(albedo.a < 0.1) discard;

    vec3 nSample = texture(normal, texCoords).rgb;
    vec3 tangentNorm = normalize(nSample * 2.0 - 1.0);

    gRoughness = texture(roug, texCoords).bb;

    float lit = 0.12 + 0.88 * clamp(out_light, 0.0, 1.0);
    gAlbedo = albedo.rgb * lit;
    gNormal = normalize(TBN * tangentNorm);
    gViewPosition = vec4(out_view_pos, 1.0);
}