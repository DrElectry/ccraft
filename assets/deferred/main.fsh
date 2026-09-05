#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gRoughness;
uniform sampler2D dShadow1;
uniform sampler2D dShadow2;
uniform sampler2D dSSAO;
uniform sampler2D dSSR;

uniform sampler2D gWaterAlbedo;
uniform sampler2D gWaterDepth;
uniform sampler2D gWaterNormal;
uniform sampler2D gWaterRoughness;

uniform sampler2D caustics;
uniform sampler2D cloudAlbedo;
uniform sampler2D cloudDepth;

uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform mat4 light_space_matrix_near;
uniform mat4 light_space_matrix_far;

uniform vec3 lightDir1;
uniform vec3 lightDir2;
uniform vec3 lightColor;

uniform float shadowSplitDistance;

uniform float time;
uniform int underwater;

in vec2 out_uv;
out vec4 fragColor;

// too lazy to use uniforms

const vec3 FOG_COLOR = vec3(0.6, 0.7, 0.8); // will remove this when i switch to biomes
const float FOG_START = 110.0;
const float FOG_END = 120.0;

const vec3 SUN_COLOR = vec3(1.0, 0.95, 0.85);
const float SUN_INTENSITY = 512.0;

const float CAUSTICS_SCALE = 0.25;
const float CAUSTICS_SPEED = 0.04;
const float CAUSTICS_STRENGTH = 1.25;
const float CAUSTICS_MAX_DEPTH = 24.0;
const float CAUSTICS_CHROMATIC_ABERRATION = 0.004;

const float CHROMATIC_ABERRATION_UNDERWATER = 0.0008;

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
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
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

    float sunDisk = 1.0 - smoothstep(0.0, 0.05, angle);

    float innerGlow = 0.0;
    if (angle < 0.5) {
        float t = angle / 0.5;
        innerGlow = pow(1.0 - t, 1.5) * 0.05;
    }

    vec3 diskColor = SUN_COLOR;
    vec3 innerColor = SUN_COLOR * 1.2;

    return (diskColor * sunDisk + innerColor * innerGlow) * 4.0;
}

float pcf(vec4 fragPosLightSpace, sampler2D shadowMap, vec2 uv, float radiusMultiplier, vec3 lightDir, float receiverDist)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 0.0;

    if (proj.z > 1.0)
        return 0.0;

    float currentDepth = proj.z;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    mat2 rot = getRotation(uv);

    vec3 normal = normalize(texture(gNormal, uv).rgb * 2.0 - 1.0);
    float NdotL = dot(normal, normalize(lightDir));

    float bias;
    if (radiusMultiplier > 0.5) {
        bias = 0.00005 + 0.000015 * (1.0 - NdotL) + texelSize.x * 0.5;
    } else {
        bias = 0.001 + 0.0005 * (1.0 - NdotL) + texelSize.x * 3.0;
    }

    float occluderDist = 0.0;
    int sampleCount = 8;
    float nearestOccluder = 1.0;
    
    for (int i = 0; i < sampleCount; i++)
    {
        vec2 offset = rot * poissonDisk[i] * texelSize * 2.0;
        float sampledDepth = texture(shadowMap, proj.xy + offset).r;
        
        if (sampledDepth + bias < currentDepth) {
            float distToOccluder = currentDepth - sampledDepth;
            nearestOccluder = min(nearestOccluder, distToOccluder);
        }
    }
    
    if (nearestOccluder < 1.0) {
        occluderDist = nearestOccluder * 200.0;
        occluderDist = clamp(occluderDist, 0.5, 10.0);
    } else {
        occluderDist = 1.0;
    }

    float cameraDistFactor = 1.0 + receiverDist * 0.008;
    cameraDistFactor = clamp(cameraDistFactor, 0.5, 3.0);
    
    float radius = 2.0 * radiusMultiplier * occluderDist * cameraDistFactor;
    radius = clamp(radius, 0.5, 126.0);

    int totalSamples = 32;
    if (radius < 2.0) {
        totalSamples = 12;
    } else if (radius < 4.0) {
        totalSamples = 20;
    } else if (radius < 7.0) {
        totalSamples = 28;
    } else {
        totalSamples = 32;
    }
    int edgeSamples = 4;

    float shadow = 0.0;
    int i = 0;

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
    float shadowNear = pcf(fragPosLightSpaceNear, dShadow1, uv, 1.0, lightDir1, dist);

    vec4 fragPosLightSpaceFar = light_space_matrix_far * vec4(worldPos, 1.0);
    float shadowFar = pcf(fragPosLightSpaceFar, dShadow2, uv, 0.5, lightDir2, dist);

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

vec3 lightDirAtDist(float dist)
{
    return normalize(mix(lightDir1, lightDir2, clamp((dist - (shadowSplitDistance - 8.0)) / 16.0, 0.0, 1.0)));
}

vec3 sampleCausticsPlanarChromatic(vec3 worldPos, vec3 normal, float timeVal)
{
    vec2 uv = worldPos.xz * CAUSTICS_SCALE;
    uv += vec2(timeVal * CAUSTICS_SPEED, timeVal * CAUSTICS_SPEED * 0.7);

    vec3 tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    if (length(tangent) < 0.01)
        tangent = vec3(1.0, 0.0, 0.0);

    float scale = CAUSTICS_CHROMATIC_ABERRATION;
    float r = texture(caustics, uv + tangent.xz * scale).r;
    float g = texture(caustics, uv).g;
    float b = texture(caustics, uv - tangent.xz * scale).b;

    return vec3(r, g, b);
}

vec3 getUnderwaterFogColor()
{
    vec2 texelSize = 1.0 / vec2(textureSize(gWaterAlbedo, 0));
    vec3 waterColor = texture(gWaterAlbedo, out_uv).rgb;
    float waterPresent = dot(waterColor, vec3(0.333));

    if (waterPresent > 0.01) {
        return waterColor;
    }

    vec3 accumulatedColor = vec3(0.0);
    float totalWeight = 0.0;

    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 sampleUV = out_uv + vec2(x, y) * texelSize * 4.0;
            sampleUV = clamp(sampleUV, 0.001, 0.999);
            vec3 sampleColor = texture(gWaterAlbedo, sampleUV).rgb;
            float weight = dot(sampleColor, vec3(0.333)) > 0.01 ? 1.0 : 0.0;
            accumulatedColor += sampleColor * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0) {
        return accumulatedColor / totalWeight;
    }

    vec3 centerColor = texture(gWaterAlbedo, vec2(0.5, 0.5)).rgb;
    if (dot(centerColor, vec3(0.333)) > 0.01) {
        return centerColor;
    }

    return vec3(0.1, 0.3, 0.6);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 computeSpecular(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 lightCol, float shadow)
{
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 spec = numerator / denominator;

    spec *= lightCol * NdotL * (1.0 - shadow);

    return spec;
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

    vec3 waterAlbedo = texture(gWaterAlbedo, out_uv).rgb;
    vec3 waterNormal = normalize(texture(gWaterNormal, out_uv).rgb * 2.0 - 1.0);
    float waterRoughness = texture(gWaterRoughness, out_uv).r;
    float waterMetallic = 0.0;

    vec3 terrainAlbedo = texture(gAlbedo, out_uv).rgb;
    vec3 terrainNormal = normalize(texture(gNormal, out_uv).rgb * 2.0 - 1.0);
    float terrainRoughness = 1.0-texture(gRoughness, out_uv).r;
    float terrainMetallic = texture(gRoughness, out_uv).r;

    vec3 worldPos;
    vec3 color;
    bool isSky = false;

    vec3 cameraPos = getCameraPos();
    bool isUnderwater = (underwater != 0);

    // Calculate sky gradient for fog color
    vec3 initialRd = normalize(reconstructWorldPosition(max(terrainDepth, waterDepth)) - cameraPos);
    float fogHorizonBlend = 1.0 - max(0.0, initialRd.y);
    vec3 fogHorizonColor = vec3(0.6, 0.7, 0.8);
    vec3 fogZenithColor = vec3(0.45, 0.56, 0.66);
    vec3 skyFogColor = mix(fogZenithColor, fogHorizonColor, fogHorizonBlend);

    vec3 fogColor;
    float fogStart;
    float fogEnd;
    if (isUnderwater) {
        fogColor = vec3(0.20, 0.40, 0.44);
        fogStart = 4.0;
        fogEnd   = 22.0;
    } else {
        fogColor = skyFogColor; // Use sky gradient for fog color
        fogStart = FOG_START;
        fogEnd   = FOG_END;
    }

    vec3 light = lightColor;

    if (isWater)
    {
        vec3 waterWorldPos = reconstructWorldPosition(waterDepth);
        vec4 waterSSR = texture(dSSR, out_uv);

        float shadow = calculateShadow(waterWorldPos, out_uv);
        vec3 lightDir = lightDirAtDist(length(waterWorldPos - cameraPos));
        float NdotL = max(dot(waterNormal, lightDir), 0.0);
        float ao = texture(dSSAO, out_uv).r;

        vec3 viewDir = normalize(cameraPos - waterWorldPos);

        vec3 ambient = waterAlbedo * ao * 0.25;
        vec3 diffuse = waterAlbedo * light * NdotL * (1.0 - shadow);

        vec3 specular = computeSpecular(waterAlbedo, waterMetallic, waterRoughness, waterNormal, viewDir, lightDir, light, shadow);

        vec3 waterColor = ambient + diffuse + specular + waterSSR.rgb * waterSSR.a;

        float distToCamera = length(waterWorldPos - cameraPos);
        float depthStrength = clamp(1.0 - (distToCamera - 10.0) / 40.0, 0.0, 1.0);
        float distortionScale = 0.008 * depthStrength;
        vec2 distortion;
        distortion.x = sin(out_uv.y * 28.0 + time * 1.6) + sin(out_uv.x * 17.0 - time * 2.1) * 0.5;
        distortion.y = cos(out_uv.x * 31.0 - time * 1.8) + cos(out_uv.y * 23.0 + time * 1.4) * 0.5;
        distortion *= distortionScale;
        vec2 refractUV = clamp(out_uv + distortion, 0.001, 0.999);

        float terrainDepthRefract = texture(gDepth, refractUV).r;
        vec3 terrainWorldPosRefract = reconstructWorldPositionUV(terrainDepthRefract, refractUV);
        bool terrainIsSky = terrainDepthRefract > 0.99999;

        vec3 terrainColor;
        if (terrainIsSky) {
            // Calculate fog color based on the refracted ray direction
            vec3 refractRd = normalize(terrainWorldPosRefract - cameraPos);
            float refractHorizonBlend = 1.0 - max(0.0, refractRd.y);
            terrainColor = mix(fogZenithColor, fogHorizonColor, refractHorizonBlend);
        } else {
            float distanceFromCenter = length(refractUV - 0.5) * 2.0;
            float aberrationStrength = CHROMATIC_ABERRATION_UNDERWATER * (1.0 + distanceFromCenter * 3.0);
            vec2 chromaticOffset = normalize(refractUV - 0.5 + 0.001) * aberrationStrength * 0.5;

            vec2 refractUV_r = clamp(refractUV + chromaticOffset * 2.0, 0.001, 0.999);
            vec2 refractUV_b = clamp(refractUV - chromaticOffset * 2.0, 0.001, 0.999);

            float r = texture(gAlbedo, refractUV_r).r;
            float g = texture(gAlbedo, refractUV).g;
            float b = texture(gAlbedo, refractUV_b).b;
            vec3 terrainAlbedoAtWater = vec3(r, g, b);
            vec3 terrainNormalAtWater = normalize(texture(gNormal, refractUV).rgb * 2.0 - 1.0);
            float terrainRoughnessAtWater = texture(gRoughness, refractUV).r;
            float terrainMetallicAtWater = 0.0;

            float terrainShadow = calculateShadow(terrainWorldPosRefract, refractUV);
            vec3 terrainLightDir = lightDirAtDist(length(terrainWorldPosRefract - cameraPos));
            float terrainNdotL = max(dot(terrainNormalAtWater, terrainLightDir), 0.0);
            float terrainAO = texture(dSSAO, refractUV).r;

            vec3 terrainViewDir = normalize(cameraPos - terrainWorldPosRefract);

            vec3 terrainAmbient = terrainAlbedoAtWater * terrainAO * 0.25;
            vec3 terrainDiffuse = terrainAlbedoAtWater * light * terrainNdotL * (1.0 - terrainShadow);

            vec3 terrainSpecular = computeSpecular(terrainAlbedoAtWater, terrainMetallicAtWater, terrainRoughnessAtWater, terrainNormalAtWater, terrainViewDir, terrainLightDir, light, terrainShadow);

            terrainColor = terrainAmbient + terrainDiffuse + terrainSpecular;
            vec4 terrainSSR = texture(dSSR, refractUV);
            terrainColor += terrainSSR.rgb * terrainSSR.a;

            float waterDepthHere = abs(terrainWorldPosRefract.y - waterWorldPos.y);
            float causticFactor = smoothstep(CAUSTICS_MAX_DEPTH, 0.0, waterDepthHere);

            if (causticFactor > 0.001 && terrainShadow < 0.9) {
                vec3 causticColor = sampleCausticsPlanarChromatic(terrainWorldPosRefract, terrainNormalAtWater, time);
                causticColor = pow(causticColor, vec3(0.7));
                causticColor = smoothstep(vec3(0.1), vec3(0.9), causticColor);
                float shadowFactor = 1.0 - smoothstep(0.1, 0.9, terrainShadow);
                float causticUpFactor = max(terrainNormalAtWater.y, 0.0);
                vec3 causticLight = causticColor * light * CAUSTICS_STRENGTH * causticFactor * shadowFactor * causticUpFactor;
                terrainColor += causticLight;
            }
        }

        float distToTerrain = abs(terrainWorldPosRefract.y - waterWorldPos.y);
        float depthFactor = smoothstep(0.0, 1.5, distToTerrain);
        float terrainVisibility = depthFactor * 0.5;

        float waterDist = length(waterWorldPos - cameraPos);
        float waterFogFactor = clamp((fogEnd - waterDist) / (fogEnd - fogStart), 0.0, 1.0);
        vec3 waterFogged = mix(fogColor, waterColor, waterFogFactor);

        float terrainDist = length(terrainWorldPosRefract - cameraPos);
        float terrainFogFactor = clamp((fogEnd - terrainDist) / (fogEnd - fogStart), 0.0, 1.0);
        
        // Calculate fog color for terrain based on its position
        vec3 terrainRd = normalize(terrainWorldPosRefract - cameraPos);
        float terrainHorizonBlend = 1.0 - max(0.0, terrainRd.y);
        vec3 terrainFogColor = isUnderwater ? fogColor : mix(fogZenithColor, fogHorizonColor, terrainHorizonBlend);
        vec3 terrainFogged = mix(terrainFogColor, terrainColor, terrainFogFactor);

        color = mix(waterFogged, terrainFogged, 0.5);
        isSky = terrainIsSky && terrainVisibility > 0.5;

        worldPos = waterWorldPos;
        depth = waterDepth;
        albedo = waterAlbedo;
        normal = waterNormal;
        roughness = waterRoughness;
        metallic = waterMetallic;
    }
    else
    {
        depth = terrainDepth;

        if (isUnderwater) {
            vec2 centerOffset = out_uv - 0.5;
            float distanceFromCenter = length(centerOffset) * 2.0;
            float aberrationStrength = CHROMATIC_ABERRATION_UNDERWATER * (1.0 + distanceFromCenter * 3.0);
            vec2 offset = normalize(centerOffset + 0.001) * aberrationStrength * 0.5;
            vec2 uv_r = clamp(out_uv + offset * 2.0, 0.001, 0.999);
            vec2 uv_b = clamp(out_uv - offset * 2.0, 0.001, 0.999);
            float r = texture(gAlbedo, uv_r).r;
            float g = texture(gAlbedo, out_uv).g;
            float b = texture(gAlbedo, uv_b).b;
            terrainAlbedo = vec3(r, g, b);
        } else {
            terrainAlbedo = texture(gAlbedo, out_uv).rgb;
        }

        normal = terrainNormal;
        roughness = terrainRoughness;
        metallic = terrainMetallic;
        shadowUV = out_uv;
        worldPos = reconstructWorldPosition(depth);
        isSky = depth > 0.99999;

        float shadow = calculateShadow(worldPos, shadowUV);
        vec3 lightDir = lightDirAtDist(length(worldPos - cameraPos));
        float NdotL = max(dot(normal, lightDir), 0.0);
        float ao = texture(dSSAO, out_uv).r;
        vec4 ssr = texture(dSSR, out_uv);

        vec3 viewDir = normalize(cameraPos - worldPos);

        float ambientStrength = isUnderwater ? 0.45 : 0.25;
        vec3 ambient = terrainAlbedo * ao * ambientStrength;
        vec3 diffuse = terrainAlbedo * light * NdotL * (1.0 - shadow);

        vec3 specular = computeSpecular(terrainAlbedo, metallic, roughness, normal, viewDir, lightDir, light, shadow);

        color = ambient + diffuse + specular;

        if (!isSky)
            color += ssr.rgb * ssr.a;

        float dist = length(worldPos - cameraPos);
        float fogFactor = clamp((fogEnd - dist) / (fogEnd - fogStart), 0.0, 1.0);
        
        // Calculate fog color based on current fragment position
        vec3 currentRd = normalize(worldPos - cameraPos);
        float currentHorizonBlend = 1.0 - max(0.0, currentRd.y);
        vec3 currentFogColor = isUnderwater ? fogColor : mix(fogZenithColor, fogHorizonColor, currentHorizonBlend);
        
        color = mix(currentFogColor, color, fogFactor);
    }

    vec3 rd = getRd(worldPos, cameraPos);
    float dist = length(worldPos - cameraPos);
    vec3 lightDir = lightDirAtDist(dist);

    if (isSky && !(isUnderwater && depth > 0.99999))
    {
        vec3 sunColor = getSun(rd, lightDir);
        
        float horizonBlend = 1.0 - max(0.0, rd.y);
        vec3 horizonColor = vec3(0.6, 0.7, 0.8);
        vec3 zenithColor = vec3(0.45, 0.56, 0.66);
        vec3 skyGradient = mix(zenithColor, horizonColor, horizonBlend);
        
        color = skyGradient + sunColor;
    }

    vec2 cloudUV = out_uv;
    vec4 cloudColor = texture(cloudAlbedo, cloudUV);
    float cloudAlpha = cloudColor.a;
    float cloudDepthVal = texture(cloudDepth, cloudUV).r;
    
    if (cloudAlpha > 0.01) {
        vec3 cloudWorldPos = reconstructWorldPositionUV(cloudDepthVal, cloudUV);
        float cloudDist = length(cloudWorldPos - cameraPos);
        float terrainDist = length(worldPos - cameraPos);
        
        if (cloudDepthVal > 0.0 && cloudDist < terrainDist && cloudDist < fogEnd) {
            float cloudShadow = calculateShadow(cloudWorldPos, cloudUV);
            vec3 cloudLightDir = lightDirAtDist(cloudDist);
            float cloudNdotL = max(dot(normalize(vec3(0.0, 1.0, 0.0)), cloudLightDir), 0.0);
            float cloudAO = texture(dSSAO, cloudUV).r;
            
            vec3 cloudViewDir = normalize(cameraPos - cloudWorldPos);
            vec3 cloudAmbient = cloudColor.rgb * cloudAO * 0.35;
            vec3 cloudDiffuse = cloudColor.rgb * light * cloudNdotL * (1.0 - cloudShadow) * 0.8;
            
            // Calculate fog color for clouds based on their position
            vec3 cloudRd = normalize(cloudWorldPos - cameraPos);
            float cloudHorizonBlend = 1.0 - max(0.0, cloudRd.y);
            vec3 cloudFogColor = isUnderwater ? fogColor : mix(fogZenithColor, fogHorizonColor, cloudHorizonBlend);
            
            float cloudFogFactor = clamp((fogEnd - cloudDist) / (fogEnd - fogStart), 0.0, 1.0);
            vec3 cloudFogged = mix(cloudFogColor, cloudAmbient + cloudDiffuse, cloudFogFactor);
            
            float cloudOpacity = cloudAlpha * 0.7;
            color = mix(color, cloudFogged, cloudOpacity * (1.0 - cloudShadow * 0.5));
        }
    }

    fragColor = vec4(color, 1.0);
}