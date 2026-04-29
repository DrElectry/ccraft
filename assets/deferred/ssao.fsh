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
        float angle = float(i) * 6.28318 / float(samples);
        float radius = sqrt(float(i) / float(samples)) * ssaoRadius;
        
        vec3 offset = vec3(cos(angle) * radius, sin(angle) * radius, sin(angle * 2.0) * radius * 0.3);
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
    occ = clamp(occ, 0.0, 1.0);

    fragColor = 1.0 - occ;
}