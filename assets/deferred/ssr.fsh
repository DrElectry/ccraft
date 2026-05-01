#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gRoughness;

uniform mat4 invView;
uniform mat4 projection;
uniform mat4 invprojection;
uniform mat4 view;

in vec2 out_uv;

out vec4 fragColor;

const float step = 0.1;
const float minRayStep = 0.1;
const float maxSteps = 30;
const int numBinarySearchSteps = 5;
const float reflectionSpecularFalloffExponent = 3.0;

float Metallic;

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth);
vec4 RayMarch(vec3 dir, inout vec3 hitCoord, out float dDepth);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 SSRBlur(vec2 uv);

void main()
{
    Metallic = texture(gRoughness, out_uv).r;

    vec3 worldNormal = normalize(texture(gNormal, out_uv).rgb);
    vec3 viewNormal  = normalize(vec3(view * vec4(worldNormal, 0.0)));

    vec3 viewPos = textureLod(gPosition, out_uv, 2).xyz;
    vec3 albedo  = texture(gAlbedo, out_uv).rgb;

    vec3 F0      = mix(vec3(0.04), albedo, Metallic);
    vec3 Fresnel = fresnelSchlick(
        max(dot(normalize(viewNormal), normalize(viewPos)), 0.0),
        F0
    );

    vec3 reflected = normalize(reflect(normalize(viewPos), normalize(viewNormal)));

    vec3 hitPos = viewPos;
    float dDepth;

    vec4 coords = RayMarch(reflected * max(minRayStep, -viewPos.z), hitPos, dDepth);

    vec2 dCoords = smoothstep(0.2, 0.6, abs(vec2(0.5) - coords.xy));
    float screenEdgeFactor = clamp(1.0 - (dCoords.x + dCoords.y), 0.0, 1.0);

    float ReflectionMultiplier = pow(Metallic, reflectionSpecularFalloffExponent)
                               * screenEdgeFactor
                               * -reflected.z;

    vec3 SSR = SSRBlur(coords.xy)
             * clamp(ReflectionMultiplier, 0.0, 0.9)
             * Fresnel;

    fragColor = vec4(SSR, Metallic);
}

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth)
{
    float depth;
    vec4 projectedCoord;

    for (int i = 0; i < numBinarySearchSteps; i++)
    {
        projectedCoord = projection * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;

        depth  = textureLod(gPosition, projectedCoord.xy, 2).z;
        dDepth = hitCoord.z - depth;

        dir *= 0.5;
        if (dDepth > 0.0)
            hitCoord += dir;
        else
            hitCoord -= dir;
    }

    projectedCoord = projection * vec4(hitCoord, 1.0);
    projectedCoord.xy /= projectedCoord.w;
    projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;

    return vec3(projectedCoord.xy, depth);
}

vec4 RayMarch(vec3 dir, inout vec3 hitCoord, out float dDepth)
{
    dir *= step;

    float depth;
    vec4 projectedCoord;

    for (int i = 0; i < maxSteps; i++)
    {
        hitCoord += dir;

        projectedCoord = projection * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;

        depth = textureLod(gPosition, projectedCoord.xy, 2).z;

        if (depth > 1000.0)
            continue;

        dDepth = hitCoord.z - depth;

        if ((dir.z - dDepth) < 1.2)
        {
            if (dDepth <= 0.0)
            {
                return vec4(BinarySearch(dir, hitCoord, dDepth), 1.0);
            }
        }
    }

    return vec4(projectedCoord.xy, depth, 0.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 SSRBlur(vec2 uv)
{
    vec2 texel = 1.0 / vec2(textureSize(gAlbedo, 0));

    vec3 result = vec3(0.0);

    float radius = 6.0;

    float w[7] = float[](
        0.05,
        0.09,
        0.12,
        0.16,
        0.12,
        0.09,
        0.05
    );
    
    vec3 hsum = vec3(0.0);
    for (int i = -3; i <= 3; i++)
    {
        float w_i = w[i + 3];
        vec2 offset = vec2(float(i)) * texel * radius;
        hsum += texture(gAlbedo, uv + offset).rgb * w_i;
    }

    vec3 vsum = vec3(0.0);
    for (int i = -3; i <= 3; i++)
    {
        float w_i = w[i + 3];
        vec2 offset = vec2(0.0, float(i)) * texel * radius;
        vsum += hsum * w_i;
    }

    return vsum;
}