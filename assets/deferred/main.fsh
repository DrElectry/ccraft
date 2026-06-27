#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D dShadow1;
uniform sampler2D dShadow2;
uniform sampler2D dSSAO;
uniform sampler2D dSSR;

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
uniform float waterHeight;

in vec2 out_uv;
out vec4 fragColor;

const vec3 FOG_COLOR = vec3(0.6, 0.7, 0.8);
const float FOG_START = 110.0;
const float FOG_END = 120.0;

const vec3 SUN_COLOR = vec3(1.0, 0.95, 0.85);
const float SUN_INTENSITY = 512.0;

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

    vec3 diskColor = SUN_COLOR;
    vec3 innerColor = SUN_COLOR * 1.2;

    return (diskColor * sunDisk + innerColor * innerGlow) * 4.0;
}

float pcf(vec4 fragPosLightSpace, sampler2D shadowMap, vec2 uv, float radiusMultiplier, vec3 lightDir)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    if( proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 )
        return 0.0;

    if(proj.z > 1.0)
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

    float radius = 2.0 * radiusMultiplier;
    int totalSamples = (radiusMultiplier > 0.5) ? 16 : 32;
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
    float shadowNear = pcf(fragPosLightSpaceNear, dShadow1, uv, 1.0, lightDir1);

    vec4 fragPosLightSpaceFar = light_space_matrix_far * vec4(worldPos, 1.0);
    float shadowFar = pcf(fragPosLightSpaceFar, dShadow2, uv, 0.5, lightDir2);

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
    float waterRoughness = texture(dSSR, out_uv).g;
    float waterMetallic = texture(dSSR, out_uv).b;

    vec3 terrainAlbedo = texture(gAlbedo, out_uv).rgb;
    vec3 terrainNormal = normalize(texture(gNormal, out_uv).rgb * 2.0 - 1.0);
    float terrainRoughness = texture(dSSR, out_uv).g;
    float terrainMetallic = texture(dSSR, out_uv).b;

    vec3 worldPos;
    vec3 color;
    bool isSky = false;

    vec3 cameraPos = getCameraPos();
    bool underwater = cameraPos.y < waterHeight;

    if (isWater)
    {
        vec3 waterWorldPos = reconstructWorldPosition(waterDepth);
        vec4 waterSSR = texture(dSSR, out_uv);
        
        float shadow = calculateShadow(waterWorldPos, out_uv);
        vec3 lightDir = lightDirAtDist(length(waterWorldPos - cameraPos));
        float NdotL = max(dot(waterNormal, lightDir), 0.0);
        float ao = texture(dSSAO, out_uv).r;
        
        vec3 ambient = waterAlbedo * ao * 0.25;
        vec3 diffuse = waterAlbedo * lightColor * NdotL * (1.0 - shadow);
        vec3 waterColor = ambient + diffuse + waterSSR.rgb * waterSSR.a;

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
            terrainColor = FOG_COLOR;
        } else {
            vec3 terrainAlbedoAtWater = texture(gAlbedo, refractUV).rgb;
            vec3 terrainNormalAtWater = normalize(texture(gNormal, refractUV).rgb * 2.0 - 1.0);
            
            float terrainShadow = calculateShadow(terrainWorldPosRefract, refractUV);
            vec3 terrainLightDir = lightDirAtDist(length(terrainWorldPosRefract - cameraPos));
            float terrainNdotL = max(dot(terrainNormalAtWater, terrainLightDir), 0.0);
            float terrainAO = texture(dSSAO, refractUV).r;
            
            vec3 terrainAmbient = terrainAlbedoAtWater * terrainAO * 0.25;
            vec3 terrainDiffuse = terrainAlbedoAtWater * lightColor * terrainNdotL * (1.0 - terrainShadow);
            terrainColor = terrainAmbient + terrainDiffuse;
            vec4 terrainSSR = texture(dSSR, refractUV);
            terrainColor += terrainSSR.rgb * terrainSSR.a;
        }

        float distToTerrain = abs(terrainWorldPosRefract.y - waterWorldPos.y);
        float depthFactor = smoothstep(0.0, 1.5, distToTerrain);
        float terrainVisibility = depthFactor * 0.5;

        float waterDist = length(waterWorldPos - cameraPos);
        float waterFogFactor = clamp((FOG_END - waterDist) / (FOG_END - FOG_START), 0.0, 1.0);
        vec3 waterFogged = mix(FOG_COLOR, waterColor, waterFogFactor);

        float terrainDist = length(terrainWorldPosRefract - cameraPos);
        float terrainFogFactor = clamp((FOG_END - terrainDist) / (FOG_END - FOG_START), 0.0, 1.0);
        vec3 terrainFogged = mix(FOG_COLOR, terrainColor, terrainFogFactor);

        color = mix(waterFogged, terrainFogged, terrainVisibility);
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
        albedo = terrainAlbedo;
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

        vec3 ambient = albedo * ao * 0.25;
        vec3 diffuse = albedo * lightColor * NdotL * (1.0 - shadow);
        color = ambient + diffuse;

        if (!isSky)
            color += ssr.rgb * ssr.a;

        float dist = length(worldPos - cameraPos);
        float fogFactor = clamp((FOG_END - dist) / (FOG_END - FOG_START), 0.0, 1.0);
        color = mix(FOG_COLOR, color, fogFactor);
    }

    vec3 rd = getRd(worldPos, cameraPos);
    float dist = length(worldPos - cameraPos);
    vec3 lightDir = lightDirAtDist(dist);
    
    if (isSky && !(underwater && depth > 0.99999))
    {
        color += getSun(rd, lightDir);
    }

    fragColor = vec4(color, 1.0);
}