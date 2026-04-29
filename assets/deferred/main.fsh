#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D dShadow;

uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform mat4 light_space_matrix;

uniform vec3 lightDir;
uniform vec3 lightColor;

in vec2 out_uv;
out vec4 fragColor;

const int BLOCKER_SAMPLES = 16;
const int PCF_SAMPLES = 24;

vec2 poissonDisk[32] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790),
    vec2(-0.01967400, -0.43500000),
    vec2(0.25000000, -0.25000000),
    vec2(-0.50000000, 0.20000000),
    vec2(0.70000000, 0.10000000),
    vec2(-0.70000000, 0.70000000),
    vec2(0.10000000, 0.90000000),
    vec2(-0.30000000, -0.70000000),
    vec2(0.60000000, -0.60000000),
    vec2(-0.60000000, 0.40000000),
    vec2(0.40000000, 0.60000000),
    vec2(-0.10000000, 0.30000000),
    vec2(0.80000000, -0.20000000),
    vec2(-0.80000000, -0.10000000),
    vec2(0.20000000, -0.90000000),
    vec2(-0.40000000, 0.80000000),
    vec2(0.90000000, 0.30000000)
);

float rand(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

mat2 rot2(float a)
{
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

float findBlocker(vec2 uv, float receiverDepth, float radius, mat2 rot)
{
    float avgDepth = 0.0;
    float blockers = 0.0;

    for (int i = 0; i < BLOCKER_SAMPLES; i++)
    {
        vec2 offset = rot * poissonDisk[i] * radius;
        float depth = texture(dShadow, uv + offset).r;

        if (depth < receiverDepth)
        {
            avgDepth += depth;
            blockers += 1.0;
        }
    }

    if (blockers == 0.0)
        return -1.0;

    return avgDepth / blockers;
}

float pcf(vec2 uv, float receiverDepth, float radius, mat2 rot)
{
    float shadow = 0.0;
    float bias = 0.0015;

    for (int i = 0; i < PCF_SAMPLES; i++)
    {
        vec2 offset = rot * poissonDisk[i] * radius;
        float depth = texture(dShadow, uv + offset).r;

        shadow += step(depth, receiverDepth - bias);
    }

    return shadow / float(PCF_SAMPLES);
}

float pcss(vec4 fragPosLightSpace)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (
        proj.z > 1.0 ||
        proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0
    )
    {
        return 0.0;
    }

    float receiverDepth = proj.z;

    vec2 texelSize = 1.0 / vec2(textureSize(dShadow, 0));

    float lightSize = 0.03;

    float searchRadius =
        max(
            lightSize * (receiverDepth - 0.02) / max(receiverDepth, 0.0001),
            texelSize.x * 2.0
        );

    float angle = rand(gl_FragCoord.xy) * 6.2831853;
    mat2 rot = rot2(angle);

    float blocker =
        findBlocker(
            proj.xy,
            receiverDepth,
            searchRadius,
            rot
        );

    if (blocker < 0.0)
        return 0.0;

    float penumbra =
        clamp(
            (receiverDepth - blocker) / blocker,
            0.0,
            1.0
        );

    float radius =
        max(
            penumbra * lightSize,
            texelSize.x * 1.5
        );

    return pcf(
        proj.xy,
        receiverDepth,
        radius,
        rot
    );
}

vec3 reconstructWorldPosition(float depth)
{
    vec4 ndc = vec4(
        out_uv * 2.0 - 1.0,
        depth * 2.0 - 1.0,
        1.0
    );

    vec4 viewPos = inv_projection * ndc;
    viewPos /= viewPos.w;

    return (inv_view * viewPos).xyz;
}

void main()
{
    vec3 albedo = texture(gAlbedo, out_uv).rgb;

    float depth = texture(gDepth, out_uv).r;

    vec3 normal =
        normalize(
            texture(gNormal, out_uv).rgb * 2.0 - 1.0
        );

    vec3 worldPos = reconstructWorldPosition(depth);

    vec4 fragPosLightSpace =
        light_space_matrix * vec4(worldPos, 1.0);

    float shadow = pcss(fragPosLightSpace);

    vec3 L = normalize(lightDir);

    float diffuse =
        max(dot(normal, L), 0.0);

    vec3 ambient =
        albedo * 0.35;

    vec3 direct =
        albedo *
        lightColor *
        diffuse *
        (1.0 - shadow);

    vec3 lighting =
        ambient + direct;
    fragColor = vec4(lighting, 1.0);
    return;
}