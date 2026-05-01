#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D dShadow;
uniform sampler2D dSSAO;
uniform sampler2D dSSR;

uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform mat4 light_space_matrix;

uniform vec3 lightDir;
uniform vec3 lightColor;

in vec2 out_uv;
out vec4 fragColor;

vec2 poissonDisk[16] = vec2[](
vec2(-0.942, -0.399),
vec2(0.945, -0.768),
vec2(-0.094, -0.929),
vec2(0.344, 0.293),
vec2(-0.815, 0.457),
vec2(-0.815, -0.879),
vec2(-0.382, 0.276),
vec2(0.974, 0.756),
vec2(0.443, -0.975),
vec2(0.537, -0.473),
vec2(-0.264, -0.418),
vec2(0.791, 0.190),
vec2(-0.241, 0.997),
vec2(-0.814, 0.914),
vec2(0.199, 0.786),
vec2(0.143, -0.141)
);

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898,78.233))) * 43758.5453);
}

mat2 getRotation(vec2 uv)
{
    float a = rand(uv) * 6.2831853;
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

vec3 reconstructWorldPosition(float depth)
{
    vec4 ndc = vec4(out_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = inv_projection * ndc;
    viewPos /= viewPos.w;
    return (inv_view * viewPos).xyz;
}

float pcf(vec4 fragPosLightSpace)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    
    if (proj.z > 1.0)
        return 0.0;
    
    float currentDepth = proj.z;
    vec2 texelSize = 1.0 / vec2(textureSize(dShadow, 0));
    mat2 rot = getRotation(out_uv);
    
    vec3 normal = normalize(texture(gNormal, out_uv).rgb * 2.0 - 1.0);
    float bias = max(0.002 * (1.0 - dot(normal, normalize(lightDir))), 0.0005);
    
    float shadow = 0.0;
    float radius = 1.5;
    
    for (int i = 0; i < 16; i++)
    {
        vec2 offset = rot * poissonDisk[i] * texelSize * radius;
        float closestDepth = texture(dShadow, proj.xy + offset).r;
        shadow += step(closestDepth, currentDepth - bias);
    }
    
    return shadow / 16.0;
}

// Improved fresnel with roughness
float fresnelSchlick(float cosTheta, float roughness)
{
    return pow(1.0 - max(cosTheta, 0.0), 5.0) * (1.0 - roughness) + roughness;
}

void main()
{
    // Get base color components
    float ao = texture(dSSAO, out_uv).r;
    vec3 albedo = texture(gAlbedo, out_uv).rgb;
    float depth = texture(gDepth, out_uv).r;
    vec3 normal = normalize(texture(gNormal, out_uv).rgb * 2.0 - 1.0);
    
    // Check for sky/background
    bool isSky = depth > 0.9999;
    
    // Reconstruct world position
    vec3 worldPos = reconstructWorldPosition(depth);
    vec4 fragPosLightSpace = light_space_matrix * vec4(worldPos, 1.0);
    
    // Calculate shadow
    float shadow = pcf(fragPosLightSpace);
    
    // Light calculations
    vec3 L = normalize(lightDir);
    vec3 V = normalize(-worldPos);
    vec3 H = normalize(L + V);
    
    float NdotL = max(dot(normal, L), 0.0);
    float NdotV = max(dot(normal, V), 0.0);
    float NdotH = max(dot(normal, H), 0.0);
    
    // Improved fresnel with surface roughness
    float roughness = 0.1; // Could be read from material
    float fresnel = fresnelSchlick(NdotV, roughness);
    
    // Ambient with AO
    vec3 ambient = albedo * ao * 0.5;
    
    // Diffuse lighting
    vec3 diffuse = albedo * lightColor * NdotL * (1.0 - shadow);
    
    // Combine base color
    vec3 color = ambient + diffuse;
    
    // Blend SSR with smooth transition
    vec4 ssr = texture(dSSR, out_uv);
    vec3 ssrColor = ssr.rgb;
    float ssrAlpha = ssr.a;
    
    // Smooth blend: use alpha from SSR along with fresnel
    float ssrStrength = fresnel * clamp(ssrAlpha + 0.2, 0.0, 1.0);
    
    if (!isSky && ssrAlpha > 0.01) {
        // Smooth interpolation between base color and SSR
        color = mix(color, ssrColor, ssrStrength * 0.8);
    }
    else if (isSky) {
        // For sky, use a simple reflection approximation
        vec3 reflectedDir = reflect(normalize(worldPos), normal);
        float skyFactor = max(reflectedDir.y * 0.5 + 0.5, 0.0);
        vec3 skyColor = mix(vec3(0.6, 0.7, 0.8), vec3(0.3, 0.5, 0.8), skyFactor);
        
        // Apply subtle sky reflection based on fresnel
        color = mix(color, skyColor * albedo, fresnel * 0.3);
    }
    
    // Add subtle specular highlight
    float spec = pow(NdotH, 32.0 / max(roughness, 0.01)) * (1.0 - shadow);
    color += lightColor * spec * 0.25;
    
    fragColor = vec4(color, 1.0);
}
