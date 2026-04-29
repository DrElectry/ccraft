#version 330 core

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gNoise;
uniform mat4 inverseProjection;
uniform mat4 proj;

in vec2 out_uv;
out float fragColor;

const float ssaoRadius = 0.1;
const float bias = 0.0025;
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

vec2 clipToTexture(vec2 clip)
{
    return clip * 0.5 + 0.5;
}

void main()
{
    float d0 = texture(gDepth, out_uv).r;
    if (d0 >= 0.9999)
    {
        fragColor = 1.0;
        return;
    }

    vec3 p0 = clipToView(out_uv, d0);
    vec3 n0_raw = texture(gNormal, out_uv).rgb;
    vec3 n0 = normalize(n0_raw * 2.0 - 1.0);

    vec3 r = texture(gNoise, out_uv * 8.0).rgb;
    vec3 tangent = normalize(r - n0 * dot(r, n0));
    vec3 bitangent = cross(n0, tangent);
    mat3 tbn = mat3(tangent, bitangent, n0);

    float occ = 0.0;

    for (int i = 0; i < samples; i++)
    {
        vec3 offset = ssaoKernel[i];
        vec3 s = p0 + tbn * offset;

        vec4 clip = proj * vec4(s, 1.0);
        clip.xyz /= clip.w;
        vec2 uv = clipToTexture(clip.xy);

        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            continue;

        float d = texture(gDepth, uv).r;
        if (d >= 0.9999) continue;

        vec3 p = clipToView(uv, d);

        float depthDiff = p.z - p0.z;
        float range = smoothstep(0.0, 1.0, ssaoRadius / abs(p0.z - p.z + 0.0001));
        float occlusion = range * step(bias, depthDiff);

        occ += occlusion;
    }

    occ /= float(samples);
    occ*=0.6;
    occ = clamp(occ, 0.0, 1.0);

    fragColor = 1.0 - occ;
}