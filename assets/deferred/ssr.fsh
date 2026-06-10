#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gRoughness;
uniform sampler2D gDepth;

uniform mat4 invView;
uniform mat4 projection;
uniform mat4 invprojection;
uniform mat4 view;

in vec2 out_uv;

out vec4 fragColor;

const float step = 0.1;
const float minRayStep = 0.1;
const float maxSteps = 32;
const int numBinarySearchSteps = 8;
const float reflectionSpecularFalloffExponent = 3.0;

float Metallic;

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth);
vec4 RayMarch(vec3 dir, inout vec3 hitCoord, out float dDepth);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 SSRBlur(vec2 uv);

void main()
{
    float d0 = texture(gDepth, out_uv).r;
    if (d0 >= 0.999999) {
        fragColor = vec4(0.0);
        return;
    }
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
    
    int maxSamples = int(mix(15, 31, metallic));
    float radius = mix(1.5, 4.0, metallic);
    
    float weights[32];
    for (int i = 1; i <= 31; i++) {
        float t = float(i) / float(maxSamples);
        weights[i] = exp(-t * t * 2.5) * (1.0 - t * 0.3);
    }
    
    vec3 result = texture(gAlbedo, uv).rgb;
    
    if (maxSamples > 1)
    {
        vec3 horiz = result * 0.3;
        float totalWeight = 0.3;
        
        for (int x = 1; x <= maxSamples; x++)
        {
            float weight = weights[x] * (1.0 - pow(float(x) / float(maxSamples + 1), 1.2));
            
            vec2 offsetPos = vec2(float(x) * texel.x * radius, 0.0);
            vec2 offsetNeg = vec2(-float(x) * texel.x * radius, 0.0);
            
            horiz += texture(gAlbedo, uv + offsetPos).rgb * weight;
            horiz += texture(gAlbedo, uv + offsetNeg).rgb * weight;
            totalWeight += weight * 2.0;
        }
        
        horiz /= totalWeight;
        
        vec3 vert = horiz * 0.3;
        totalWeight = 0.3;
        
        for (int y = 1; y <= maxSamples; y++)
        {
            float weight = weights[y] * (1.0 - pow(float(y) / float(maxSamples + 1), 1.2));
            
            vec2 offsetPos = vec2(0.0, float(y) * texel.y * radius);
            vec2 offsetNeg = vec2(0.0, -float(y) * texel.y * radius);
            
            vert += texture(gAlbedo, uv + offsetPos).rgb * weight;
            vert += texture(gAlbedo, uv + offsetNeg).rgb * weight;
            totalWeight += weight * 2.0;
        }
        
        result = vert / totalWeight;
    }
    
    return result;
}