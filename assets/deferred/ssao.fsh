#version 330 core

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gNoise;
uniform mat4 inverseProjection;
uniform mat4 proj;
uniform vec2 ssao_ratio;

in vec2 out_uv;
out float fragColor;

const float ssaoRadius = 1.0;
const float bias = 0.005;
const int samples = 32;

// pregenerated offline kernel
const vec3 ssaoKernel[32] = vec3[](
    vec3(0.0625, 0.0000, 0.0000),  // i=0
    vec3(0.0541, 0.0313, 0.0312),  // i=1
    vec3(0.0313, 0.0541, 0.0587),  // i=2
    vec3(0.0000, 0.0625, 0.0809),  // i=3
    vec3(-0.0313, 0.0541, 0.0961), // i=4
    vec3(-0.0541, 0.0313, 0.1033), // i=5
    vec3(-0.0625, 0.0000, 0.1025), // i=6
    vec3(-0.0541, -0.0313, 0.0947),// i=7
    vec3(-0.0313, -0.0541, 0.0814),// i=8
    vec3(0.0000, -0.0625, 0.0644), // i=9
    vec3(0.0313, -0.0541, 0.0454), // i=10
    vec3(0.0541, -0.0313, 0.0262), // i=11
    vec3(0.0884, 0.0000, 0.0000),  // i=12
    vec3(0.0765, 0.0442, 0.0441),  // i=13
    vec3(0.0442, 0.0765, 0.0830),  // i=14
    vec3(0.0000, 0.0884, 0.1144),  // i=15
    vec3(-0.0442, 0.0765, 0.1359), // i=16
    vec3(-0.0765, 0.0442, 0.1461), // i=17
    vec3(-0.0884, 0.0000, 0.1449), // i=18
    vec3(-0.0765, -0.0442, 0.1339),// i=19
    vec3(-0.0442, -0.0765, 0.1151),// i=20
    vec3(0.0000, -0.0884, 0.0911), // i=21
    vec3(0.0442, -0.0765, 0.0642), // i=22
    vec3(0.0765, -0.0442, 0.0370), // i=23
    vec3(0.1083, 0.0000, 0.0000),  // i=24
    vec3(0.0937, 0.0541, 0.0540),  // i=25
    vec3(0.0541, 0.0937, 0.1017),  // i=26
    vec3(0.0000, 0.1083, 0.1401),  // i=27
    vec3(-0.0541, 0.0937, 0.1665), // i=28
    vec3(-0.0937, 0.0541, 0.1790), // i=29
    vec3(-0.1083, 0.0000, 0.1775), // i=30
    vec3(-0.0937, -0.0541, 0.1640) // i=31
);

vec3 clipToView(vec2 uv, float d)
{
    float z = d * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 view = inverseProjection * clip;
    return view.xyz / view.w;
}

void main()
{
    vec2 scaled_uv = out_uv * ssao_ratio;
    
    float d0 = texture(gDepth, scaled_uv).r;
    if (d0 >= 0.9999)
    {
        fragColor = 1.0;
        return;
    }

    vec3 p0 = clipToView(scaled_uv, d0);
    vec3 n0 = normalize(texture(gNormal, scaled_uv).rgb * 2.0 - 1.0);

    vec3 r = texture(gNoise, out_uv * 8.0).rgb;

    vec3 tangent = normalize(r - n0 * dot(r, n0));
    vec3 bitangent = cross(n0, tangent);
    mat3 tbn = mat3(tangent, bitangent, n0);

    float occ = 0.0;

    const float invSamples = 1.0 / float(samples);

    for (int i = 0; i < samples; i++)
    {
        vec3 s = p0 + (tbn * ssaoKernel[i]);

        vec4 clip = proj * vec4(s, 1.0);
        float invW = 1.0 / clip.w;
        vec2 ndc = clip.xy * invW;
        vec2 uv = ndc * 0.5 + 0.5;

        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            continue;

        float d = texture(gDepth, uv).r;
        if (d >= 0.9999) continue;

        float sampleZ = clipToView(uv, d).z;
        float depthDiff = sampleZ - p0.z;

        if (depthDiff <= bias)
            continue;

        float ndot = max(0.0, dot(n0, normalize(tbn * ssaoKernel[i])));

        float range = smoothstep(0.0, 1.0,
            ssaoRadius / (abs(depthDiff) + 0.0001));

        occ += range * ndot;
    }

    occ *= invSamples;
    occ *= 0.6;

    fragColor = 1.0 - clamp(occ, 0.0, 1.0);
}