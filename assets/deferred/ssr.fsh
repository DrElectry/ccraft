#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gPosition;
uniform sampler2D gNormal;

uniform mat4 projection;
uniform mat4 view;

in vec2 out_uv;

out vec4 fragColor;

const float step = 0.05;
const float minRayStep = 0.05;
const int maxSteps = 64;
const int numBinarySearchSteps = 6;
const float reflectionSpecularFalloffExponent = 3.0;

// Material properties
const float Metallic = 1.0;
const float Roughness = 0.0;

const float screenEdgeFadeStart = 0.3;
const float screenEdgeFadeEnd = 0.1;

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth);
vec4 RayMarch(vec3 dir, inout vec3 hitCoord, out float dDepth);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

void main()
{
    // Get world normal and convert to view space
    vec3 worldNormal = normalize(texture(gNormal, out_uv).rgb);
    vec3 viewNormal = normalize(vec3(view * vec4(worldNormal, 0.0)));
    
    // Get view position - handle both linear and non-linear depth
    vec3 viewPos = texture(gPosition, out_uv).xyz;
    float depth = texture(gPosition, out_uv).z;
    
    // Early out for sky/background - use full alpha to indicate no valid hit
    if (depth > 0.9999 || abs(viewPos.z) > 1000.0)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    
    // Get albedo for F0 calculation
    vec3 albedo = texture(gAlbedo, out_uv).rgb;
    
    // Calculate F0 based on metallic
    vec3 F0 = mix(vec3(0.04), albedo, Metallic);
    
    // Calculate fresnel
    vec3 V = normalize(-viewPos);
    float NdotV = max(dot(viewNormal, V), 0.0);
    vec3 Fresnel = fresnelSchlick(NdotV, F0);
    
    // Calculate reflection direction in view space
    vec3 reflected = normalize(reflect(normalize(viewPos), viewNormal));
    
    // Start ray marching from current position
    vec3 hitPos = viewPos;
    float dDepth;
    
    // Ray march to find intersection
    vec4 coords = RayMarch(reflected * max(minRayStep, -viewPos.z), hitPos, dDepth);
    
    // Calculate screen edge fade factor
    vec2 dCoords = smoothstep(vec2(screenEdgeFadeStart), vec2(1.0 - screenEdgeFadeEnd), abs(vec2(0.5) - coords.xy));
    float screenEdgeFactor = 1.0 - clamp(dCoords.x + dCoords.y, 0.0, 1.0);
    
    // Calculate reflection multiplier
    float ReflectionMultiplier = pow(Metallic, reflectionSpecularFalloffExponent) 
                               * screenEdgeFactor
                               * clamp(-reflected.z, 0.0, 1.0);
    
    vec3 SSR = vec3(0.0);
    float ssrAlpha = 0.0;
    
    // If we found a valid hit
    if (coords.w > 0.0 && coords.x >= 0.0 && coords.x <= 1.0 && coords.y >= 0.0 && coords.y <= 1.0)
    {
        // Sample the albedo at the hit position
        vec3 hitAlbedo = texture(gAlbedo, coords.xy).rgb;
        
        // Calculate reflection color with fresnel
        SSR = hitAlbedo * ReflectionMultiplier * Fresnel;
        ssrAlpha = ReflectionMultiplier * coords.w;
    }
    else
    {
        // Smooth fadeout for missed reflections based on ray march distance
        // Use the alpha to create a gradient falloff
        float falloff = smoothstep(0.0, 0.5, -viewPos.z) * 0.3;
        SSR = vec3(0.0);
        ssrAlpha = falloff;
    }
    
    fragColor = vec4(SSR, ssrAlpha);
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
        
        // Skip if outside screen bounds
        if (projectedCoord.x < 0.0 || projectedCoord.x > 1.0 || projectedCoord.y < 0.0 || projectedCoord.y > 1.0)
            break;
        
        depth = texture(gPosition, projectedCoord.xy).z;
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
        
        // Bounds check
        if (projectedCoord.x < 0.0 || projectedCoord.x > 1.0 || projectedCoord.y < 0.0 || projectedCoord.y > 1.0)
        {
            return vec4(projectedCoord.xy, 0.0, 0.0);
        }
        
        depth = texture(gPosition, projectedCoord.xy).z;
        
        // Skip sky pixels
        if (depth > 0.9999)
            continue;
        
        dDepth = hitCoord.z - depth;
        
        // Hit test with adaptive thickness
        float thickness = 1.0 + 0.1 * abs(hitCoord.z);
        if (dDepth > 0.0 && dDepth < thickness)
        {
            // Run binary search for precision
            vec3 binaryDir = -dir / step;
            return vec4(BinarySearch(binaryDir, hitCoord, dDepth), 1.0);
        }
    }
    
    return vec4(projectedCoord.xy, depth, 0.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
