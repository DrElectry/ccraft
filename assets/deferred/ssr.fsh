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
    
    float metallic = clamp(Metallic, 0.0, 1.0);
    
    // metallic 0.0 = minimal blur (3 samples)
    // metallic 0.5 = medium heavy blur (9 samples)
    // metallic 1.0 = heavy blur (15 samples)
    int maxSamples = int(mix(3, 15, metallic));
    float weights[16] = float[](
        0.0,    // unused
        0.65,   // kernel size 1
        0.55,   // kernel size 2
        0.48,   // kernel size 3
        0.42,   // kernel size 4
        0.37,   // kernel size 5
        0.33,   // kernel size 6
        0.29,   // kernel size 7
        0.26,   // kernel size 8
        0.23,   // kernel size 9
        0.20,   // kernel size 10
        0.17,   // kernel size 11
        0.14,   // kernel size 12
        0.11,   // kernel size 13
        0.08,   // kernel size 14
        0.05    // kernel size 15
    );
    
    vec3 result = texture(gAlbedo, uv).rgb;
    
    if (maxSamples > 1)
    {
        vec3 horiz = result * 0.5;
        float totalWeight = 0.5;
        
        for (int x = 1; x <= maxSamples; x++)
        {
            float weight = weights[maxSamples] * (1.0 - pow(float(x) / float(maxSamples), 1.5));
            weight *= (1.0 - (float(x) / float(maxSamples + 2)));
            
            vec2 offsetPos = vec2(float(x) * texel.x * 1.5, 0.0);
            vec2 offsetNeg = vec2(-float(x) * texel.x * 1.5, 0.0);
            
            horiz += texture(gAlbedo, uv + offsetPos).rgb * weight;
            horiz += texture(gAlbedo, uv + offsetNeg).rgb * weight;
            totalWeight += weight * 2.0;
        }
        
        horiz /= totalWeight;
        vec3 vert = horiz * 0.5;
        totalWeight = 0.5;
        
        for (int y = 1; y <= maxSamples; y++)
        {
            float weight = weights[maxSamples] * (1.0 - pow(float(y) / float(maxSamples), 1.5));
            weight *= (1.0 - (float(y) / float(maxSamples + 2)));
            
            vec2 offsetPos = vec2(0.0, float(y) * texel.y * 1.5);
            vec2 offsetNeg = vec2(0.0, -float(y) * texel.y * 1.5);
            
            vert += texture(gAlbedo, uv + offsetPos).rgb * weight;
            vert += texture(gAlbedo, uv + offsetNeg).rgb * weight;
            totalWeight += weight * 2.0;
        }
        
        result = vert / totalWeight;
    }
    
    return result;
}