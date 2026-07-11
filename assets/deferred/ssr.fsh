#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gPosition1;
uniform sampler2D gPosition2;
uniform sampler2D gNormal1;
uniform sampler2D gNormal2;
uniform sampler2D gRoughness1;
uniform sampler2D gRoughness2;
uniform sampler2D gDepth1;
uniform sampler2D gDepth2;
uniform vec2 ssr_ratio;
uniform int underwater;

uniform mat4 invView;
uniform mat4 projection;
uniform mat4 invprojection;
uniform mat4 view;

in vec2 out_uv;

out vec4 fragColor;

const float step = 0.1;
const float minRayStep = 0.1;
const float maxSteps = 64;
const int numBinarySearchSteps = 64;
const float reflectionSpecularFalloffExponent = 3.0;

float Metallic;

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth);
vec4 RayMarch(vec3 dir, inout vec3 hitCoord, out float dDepth);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 SSRBlur(vec2 uv);

void main()
{
    vec2 scaled_uv = out_uv * ssr_ratio;
    
    float d0_opaque = texture(gDepth1, scaled_uv).r;
    float d0_water = texture(gDepth2, scaled_uv).r;
    
    int bufferIndex = 0;
    if (d0_water < 0.999999 && d0_water < d0_opaque) {
        bufferIndex = 1;
    }
    
    float d0 = (bufferIndex == 0) ? d0_opaque : d0_water;
    
    if (bufferIndex == 0) {
        Metallic = texture(gRoughness1, scaled_uv).r;
    } else {
        Metallic = texture(gRoughness2, scaled_uv).r;
    }
    
    if (d0 >= 0.999999 || Metallic == 0.0 || underwater == 1) {
        fragColor = vec4(0.0);
        return;
    }

    vec3 worldNormal;
    vec3 viewPos;
    
    if (bufferIndex == 0) {
        worldNormal = normalize(texture(gNormal1, scaled_uv).rgb);
        viewPos = textureLod(gPosition1, scaled_uv, 2).xyz;
    } else {
        worldNormal = normalize(texture(gNormal2, scaled_uv).rgb);
        viewPos = textureLod(gPosition2, scaled_uv, 2).xyz;
    }
    
    vec3 viewNormal = normalize(vec3(view * vec4(worldNormal, 0.0)));
    vec3 viewDir = normalize(viewPos);
    vec3 reflected = normalize(reflect(viewDir, viewNormal));
    
    float viewReflectionDot = dot(viewDir, reflected);
    
    if (viewReflectionDot > 0.995 || viewReflectionDot < 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    vec3 albedo = texture(gAlbedo, scaled_uv).rgb;
    vec3 F0 = mix(vec3(0.04), albedo, Metallic);
    vec3 Fresnel = fresnelSchlick(
        max(dot(viewNormal, viewDir), 0.0),
        F0
    );

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

        float depth_opaque = texture(gDepth1, projectedCoord.xy).r;
        float depth_water = texture(gDepth2, projectedCoord.xy).r;
        
        int hitBufferIndex = 0;
        if (depth_water < 0.999999 && depth_water < depth_opaque) {
            hitBufferIndex = 1;
        }
        
        if (hitBufferIndex == 0) {
            depth = textureLod(gPosition1, projectedCoord.xy, 2).z;
        } else {
            depth = textureLod(gPosition2, projectedCoord.xy, 2).z;
        }
        
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

        float depth_opaque = texture(gDepth1, projectedCoord.xy).r;
        float depth_water = texture(gDepth2, projectedCoord.xy).r;
        
        int hitBufferIndex = 0;
        if (depth_water < 0.999999 && depth_water < depth_opaque) {
            hitBufferIndex = 1;
        }
        
        if (hitBufferIndex == 0) {
            depth = textureLod(gPosition1, projectedCoord.xy, 2).z;
        } else {
            depth = textureLod(gPosition2, projectedCoord.xy, 2).z;
        }

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