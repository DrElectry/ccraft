
#version 330 core

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gPosition;
uniform sampler2D gNoise;

uniform mat4 proj;
uniform mat4 view;
uniform vec2 ssao_ratio;

in vec2 out_uv;
out float fragColor;

const float ssaoRadius = 0.5;
const float bias = 0.05;
const int kernelSize = 32;

const vec3 ssaoKernel[32] = vec3[](
    vec3(0.0625, 0.0000, 0.0000),
    vec3(0.0541, 0.0313, 0.0312),
    vec3(0.0313, 0.0541, 0.0587),
    vec3(0.0000, 0.0625, 0.0809),
    vec3(-0.0313, 0.0541, 0.0961),
    vec3(-0.0541, 0.0313, 0.1033),
    vec3(-0.0625, 0.0000, 0.1025),
    vec3(-0.0541, -0.0313, 0.0947),
    vec3(-0.0313, -0.0541, 0.0814),
    vec3(0.0000, -0.0625, 0.0644),
    vec3(0.0313, -0.0541, 0.0454),
    vec3(0.0541, -0.0313, 0.0262),
    vec3(0.0884, 0.0000, 0.0000),
    vec3(0.0765, 0.0442, 0.0441),
    vec3(0.0442, 0.0765, 0.0830),
    vec3(0.0000, 0.0884, 0.1144),
    vec3(-0.0442, 0.0765, 0.1359),
    vec3(-0.0765, 0.0442, 0.1461),
    vec3(-0.0884, 0.0000, 0.1449),
    vec3(-0.0765, -0.0442, 0.1339),
    vec3(-0.0442, -0.0765, 0.1151),
    vec3(0.0000, -0.0884, 0.0911),
    vec3(0.0442, -0.0765, 0.0642),
    vec3(0.0765, -0.0442, 0.0370),
    vec3(0.1083, 0.0000, 0.0000),
    vec3(0.0937, 0.0541, 0.0540),
    vec3(0.0541, 0.0937, 0.1017),
    vec3(0.0000, 0.1083, 0.1401),
    vec3(-0.0541, 0.0937, 0.1665),
    vec3(-0.0937, 0.0541, 0.1790),
    vec3(-0.1083, 0.0000, 0.1775),
    vec3(-0.0937, -0.0541, 0.1640)
);

void main()
{
    vec2 uv = out_uv * ssao_ratio;

    if (texture(gDepth, uv).r >= 0.9999)
    {
        fragColor = 1.0;
        return;
    }

    vec3 fragPos = texture(gPosition, uv).xyz;

    vec3 worldNormal = normalize(
        texture(gNormal, uv).rgb * 2.0 - 1.0
    );

    vec3 normal = normalize(mat3(view) * worldNormal);

    vec3 randomVec = normalize(
        texture(gNoise, out_uv * 8.0).xyz * 2.0 - 1.0
    );

    vec3 tangent = randomVec - normal * dot(randomVec, normal);

    if (dot(tangent, tangent) < 0.0001)
    {
        vec3 axis = abs(normal.z) < 0.99
            ? vec3(0.0, 0.0, 1.0)
            : vec3(0.0, 1.0, 0.0);

        tangent = cross(axis, normal);
    }

    tangent = normalize(tangent);

    vec3 bitangent = normalize(cross(normal, tangent));
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    float totalWeight = 0.0;

    for (int i = 0; i < kernelSize; ++i)
    {
        vec3 samplePos =
            fragPos + (TBN * ssaoKernel[i]) * ssaoRadius;

        vec4 offset = proj * vec4(samplePos, 1.0);

        if (offset.w <= 0.0)
            continue;

        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        if (offset.x < 0.0 || offset.x > 1.0 ||
            offset.y < 0.0 || offset.y > 1.0)
            continue;

        vec2 sampleUV = offset.xy;

        if (texture(gDepth, sampleUV).r >= 0.9999)
            continue;

        vec3 surfacePos = texture(gPosition, sampleUV).xyz;

        vec3 toSurface = surfacePos - fragPos;

        float normalCheck = smoothstep(
            0.0,
            0.05,
            dot(normal, toSurface)
        );

        float rangeCheck = smoothstep(
            0.0,
            1.0,
            ssaoRadius / (abs(fragPos.z - surfacePos.z) + 1e-4)
        );

        float depthOcclusion =
            surfacePos.z >= samplePos.z + bias ? 1.0 : 0.0;

        float weight = rangeCheck;

        occlusion += depthOcclusion * normalCheck * weight;
        totalWeight += weight;
    }

    fragColor = totalWeight > 0.0
        ? clamp(1.0 - occlusion / totalWeight, 0.0, 1.0)
        : 1.0;
}
