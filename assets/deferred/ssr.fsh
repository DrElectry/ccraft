#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gRoughness;
uniform sampler2D gDepth;
uniform vec2 ssr_ratio;

uniform mat4 invView;
uniform mat4 projection;
uniform mat4 invprojection;
uniform mat4 view;

in vec2 out_uv;

out vec4 fragColor;

const float step = 0.1;
const float minRayStep = 0.1;
const float maxSteps = 32;
const int numBinarySearchSteps = 32;
const float reflectionSpecularFalloffExponent = 3.0;

float Metallic;

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth);
vec4 RayMarch(vec3 dir, inout vec3 hitCoord, out float dDepth);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 SSRBlur(vec2 uv);

void main()
{
    vec2 scaled_uv = out_uv * ssr_ratio;
    
    float d0 = texture(gDepth, scaled_uv).r;
    Metallic = texture(gRoughness, scaled_uv).r;
    if (d0 >= 0.999999 || Metallic == 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    vec3 worldNormal = normalize(texture(gNormal, scaled_uv).rgb);
    vec3 viewNormal  = normalize(vec3(view * vec4(worldNormal, 0.0)));

    vec3 viewPos = textureLod(gPosition, scaled_uv, 2).xyz;
    vec3 albedo  = texture(gAlbedo, scaled_uv).rgb;

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
    vec2 texel = 1.0 / (vec2(textureSize(gAlbedo, 0)) / ssr_ratio);
    const int radius = 8;
    const int taps = 17;
    float sigma = 3.0;
    float weights[17];
    float sum = 0.0;
    for (int i = 0; i < taps; i++) {
        int offset = i - radius;
        float w = exp(-float(offset * offset) / (2.0 * sigma * sigma));
        weights[i] = w;
        sum += w;
    }
    for (int i = 0; i < taps; i++) weights[i] /= sum;

    vec3 blurred = vec3(0.0);
    for (int dy = 0; dy < taps; dy++) {
        float wy = weights[dy];
        int yOff = dy - radius;
        for (int dx = 0; dx < taps; dx++) {
            float wx = weights[dx];
            int xOff = dx - radius;
            vec2 coord = uv + vec2(float(xOff) * texel.x, float(yOff) * texel.y);
            blurred += wx * wy * texture(gAlbedo, coord).rgb;
        }
    }
    return blurred;
}