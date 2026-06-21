#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D dShadow1;
uniform sampler2D dShadow2;
uniform sampler2D dSSAO;
uniform sampler2D dSSR;
uniform sampler2D dSSRWater;

uniform sampler2D gWaterAlbedo;
uniform sampler2D gWaterDepth;
uniform sampler2D gWaterNormal;

uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform mat4 light_space_matrix_near;
uniform mat4 light_space_matrix_far;

uniform vec3 lightDir1;
uniform vec3 lightDir2;
uniform vec3 lightColor;

uniform float shadowSplitDistance;

uniform float time;

in vec2 out_uv;
out vec4 fragColor;

const vec3 FOG_COLOR = vec3(0.6, 0.7, 0.8);
const float FOG_START = 110.0;
const float FOG_END = 120.0;

const vec3 SUN_COLOR = vec3(1.0, 0.95, 0.85);
const float SUN_INTENSITY = 512.0;

// blender poison disk
vec2 poissonDisk[32] = vec2[32](
    vec2( 0.476,  0.854), vec2(-0.659, -0.670),
    vec2( 0.905, -0.270), vec2( 0.215, -0.133),
    vec2(-0.595,  0.242), vec2(-0.146,  0.519),
    vec2( 0.108, -0.930), vec2( 0.807,  0.449),
    vec2(-0.476, -0.854), vec2( 0.659,  0.670),
    vec2(-0.905,  0.270), vec2(-0.215,  0.133),
    vec2( 0.595, -0.242), vec2( 0.146, -0.519),
    vec2(-0.108,  0.930), vec2(-0.807, -0.449),
    vec2(-0.854,  0.476), vec2( 0.670, -0.659),
    vec2( 0.270,  0.905), vec2( 0.133,  0.215),
    vec2(-0.242, -0.595), vec2(-0.519, -0.146),
    vec2( 0.930,  0.108), vec2(-0.449,  0.807),
    vec2( 0.854, -0.476), vec2(-0.670,  0.659),
    vec2(-0.270, -0.905), vec2(-0.133, -0.215),
    vec2( 0.242,  0.595), vec2( 0.519,  0.146),
    vec2(-0.930, -0.108), vec2( 0.449, -0.807)
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

vec3 reconstructWorldPositionUV(float depth, vec2 uv)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = inv_projection * ndc;
    viewPos /= viewPos.w;
    return (inv_view * viewPos).xyz;
}

vec3 getCameraPos()
{
    return (inv_view * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
}

vec3 getRd(vec3 worldPos, vec3 cameraPos)
{
    return normalize(worldPos - cameraPos);
}

vec3 getSun(vec3 rd, vec3 lightDir)
{
    vec3 sunDirection = normalize(lightDir);
    float cosTheta = dot(rd, sunDirection);
    float angle = acos(cosTheta);
    
    float sunDisk = 1.0 - smoothstep(0.0, 0.02, angle);
    
    float innerGlow = 0.0;
    if (angle < 0.08) {
        float t = angle / 0.08;
        innerGlow = pow(1.0 - t, 1.5) * 0.2;
    }
    
    float total = sunDisk + innerGlow;
    
    vec3 diskColor = SUN_COLOR;
    vec3 innerColor = SUN_COLOR * 1.2;
    
    return (diskColor * sunDisk + innerColor * innerGlow) * 4.0;
}

float pcf(vec4 fragPosLightSpace, sampler2D shadowMap, vec2 uv, float radiusMultiplier)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    if( proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 )
        return 0.0;

    float currentDepth = proj.z;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    mat2 rot = getRotation(uv);

    vec3 normal = normalize(texture(gNormal, uv).rgb * 2.0 - 1.0);
    float NdotL = dot(normal, normalize(lightDir1));
    
    float bias;
    if (radiusMultiplier > 0.5) {
        bias = 0.00005 + 0.000015 * (1.0 - NdotL) + texelSize.x * 0.5;
    } else {
        bias = 0.001 + 0.0005 * (1.0 - NdotL) + texelSize.x * 3.0;
    }

    float radius = 2.0 * radiusMultiplier;
    int totalSamples = (radiusMultiplier > 0.5) ? 16 : 32;
    int edgeSamples = 4;
    
    float shadow = 0.0;
    int i = 0;

    // edge tests from some article about "pcf optimizations"
    for (; i < edgeSamples; i++)
    {
        vec2 offset = rot * poissonDisk[i] * texelSize * radius;
        float closestDepth = texture(shadowMap, proj.xy + offset).r;
        shadow += step(closestDepth + bias, currentDepth);
    }

    if (shadow == 0.0) return 0.0;
    if (shadow == float(edgeSamples)) return 1.0;

    for (; i < totalSamples; i++)
    {
        vec2 offset = rot * poissonDisk[i] * texelSize * radius;
        float closestDepth = texture(shadowMap, proj.xy + offset).r;
        shadow += step(closestDepth + bias, currentDepth);
    }

    return shadow / float(totalSamples);
}

float calculateShadow(vec3 worldPos, vec2 uv)
{
    float dist = length(worldPos - getCameraPos());
    
    float blendStart = shadowSplitDistance - 12.0;
    float blendEnd = shadowSplitDistance + 12.0;
    
    vec4 fragPosLightSpaceNear = light_space_matrix_near * vec4(worldPos, 1.0);
    float shadowNear = pcf(fragPosLightSpaceNear, dShadow1, uv, 1.0);
    
    vec4 fragPosLightSpaceFar = light_space_matrix_far * vec4(worldPos, 1.0);
    float shadowFar = pcf(fragPosLightSpaceFar, dShadow2, uv, 0.5);
    
    if (dist <= blendStart) {
        return shadowNear;
    }
    else if (dist >= blendEnd) {
        return shadowFar;
    }
    else {
        float t = (dist - blendStart) / (blendEnd - blendStart);
        t = smoothstep(0.0, 1.0, t);
        return mix(shadowNear, shadowFar, t);
    }
}

void main()
{
    float terrainDepth = texture(gDepth, out_uv).r;
    float waterDepth = texture(gWaterDepth, out_uv).r;
    bool isWater = waterDepth < terrainDepth;

    float depth;
    vec3 albedo;
    vec3 normal;
    float roughness;
    float metallic;
    vec2 shadowUV = out_uv;
    float shadowDepth;

    vec3 waterAlbedo = texture(gWaterAlbedo, out_uv).rgb;
    vec3 waterNormal = normalize(texture(gWaterNormal, out_uv).rgb * 2.0 - 1.0);
    float waterRoughness = texture(dSSRWater, out_uv).g;
    float waterMetallic = texture(dSSRWater, out_uv).b;

    vec3 terrainAlbedo = texture(gAlbedo, out_uv).rgb;
    vec3 terrainNormal = normalize(texture(gNormal, out_uv).rgb * 2.0 - 1.0);
    float terrainRoughness = texture(dSSR, out_uv).g;
    float terrainMetallic = texture(dSSR, out_uv).b;

    vec3 worldPos;

    if (isWater)
    {
        vec3 cameraPos = getCameraPos();

        vec3 terrainWorldPos = reconstructWorldPosition(terrainDepth);
        vec3 waterWorldPos = reconstructWorldPosition(waterDepth);

        float waterThickness = distance(terrainWorldPos, waterWorldPos) * 0.08;
        float depthFactor = clamp(smoothstep(0.05, 1.5, waterThickness), 0.0, 1.0);
        float distortionStrength = mix(0.004, 0.016, depthFactor);

        vec2 distortion;
        distortion.x =
            sin(out_uv.y * 28.0 + time * 1.6) +
            sin(out_uv.x * 17.0 - time * 2.1) * 0.5;

        distortion.y =
            cos(out_uv.x * 31.0 - time * 1.8) +
            cos(out_uv.y * 23.0 + time * 1.4) * 0.5;

        distortion *= distortionStrength;

        vec2 refractUV = clamp(out_uv + distortion, 0.001, 0.999);
        vec2 reflectUV = out_uv;

        float sceneDepthRefract = texture(gDepth, refractUV).r;
        bool valid = sceneDepthRefract > waterDepth;

        vec3 belowAlbedo;

        if (valid)
        {
            belowAlbedo = texture(gAlbedo, refractUV).rgb;
            shadowDepth = texture(gDepth, refractUV).r;
            shadowUV = refractUV;
            belowAlbedo = mix(belowAlbedo, vec3(dot(belowAlbedo, vec3(0.333))), 0.15);
        }
        else
        {
            belowAlbedo = texture(gAlbedo, out_uv).rgb;
            shadowDepth = texture(gDepth, out_uv).r;
            shadowUV = out_uv;
            belowAlbedo = mix(belowAlbedo, vec3(dot(belowAlbedo, vec3(0.333))), 0.15);
        }

        vec4 waterSSR = texture(dSSRWater, reflectUV);
        vec3 reflectionColor = mix(FOG_COLOR, waterSSR.rgb, waterSSR.a);

        worldPos = waterWorldPos;
        vec3 viewDir = normalize(cameraPos - worldPos);

        float fresnel = pow(1.0 - max(dot(waterNormal, viewDir), 0.0), 5.0);
        float reflectionAmount = mix(0.08, 0.9, depthFactor) * fresnel;

        vec3 refracted = mix(waterAlbedo, belowAlbedo, 0.45);
        vec3 waterColor = mix(refracted, reflectionColor, reflectionAmount);

        albedo = waterColor;
        normal = waterNormal;
        roughness = waterRoughness;
        metallic = waterMetallic;
        depth = terrainDepth;
    }
    else
    {
        depth = terrainDepth;
        albedo = terrainAlbedo;
        normal = terrainNormal;
        roughness = terrainRoughness;
        metallic = terrainMetallic;
        shadowUV = out_uv;
        shadowDepth = depth;
        worldPos = reconstructWorldPosition(depth);
    }

    bool isSky = depth > 0.99999;

    vec3 shadowWorldPos = isWater ? reconstructWorldPositionUV(shadowDepth, shadowUV) : worldPos;

    vec3 cameraPos = getCameraPos();
    vec3 viewDir = normalize(cameraPos - worldPos);

    float distToCamera = length(worldPos - cameraPos);

    float shadow = calculateShadow(shadowWorldPos, shadowUV);

    vec3 lightDir = normalize(
        mix(
            lightDir1,
            lightDir2,
            clamp((distToCamera - (shadowSplitDistance - 8.0)) / 16.0, 0.0, 1.0)
        )
    );

    float NdotL = max(dot(normal, lightDir), 0.0);

    float ao = texture(dSSAO, out_uv).r;

    vec4 ssr = isWater ? texture(dSSRWater, out_uv) : texture(dSSR, out_uv);

    vec3 ambient = albedo * ao * 0.25;
    vec3 diffuse = albedo * lightColor * NdotL * (1.0 - shadow);

    vec3 color = ambient + diffuse;

    if (!isSky)
        color += ssr.rgb * ssr.a;

    float dist = length(worldPos - cameraPos);

    float fogFactor = clamp((FOG_END - dist) / (FOG_END - FOG_START), 0.0, 1.0);

    vec3 rd = getRd(worldPos, cameraPos);
    vec3 sunColor = getSun(rd, lightDir);

    color = mix(FOG_COLOR, color, fogFactor);

    if (isSky)
        color += sunColor;

    fragColor = vec4(color, 1.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159 * denom * denom;
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) *
           GeometrySchlickGGX(NdotL, roughness);
}