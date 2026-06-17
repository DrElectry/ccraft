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
uniform sampler2D gWaterRoughness;

uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform mat4 light_space_matrix_near;
uniform mat4 light_space_matrix_far;

uniform vec3 lightDir1;
uniform vec3 lightDir2;
uniform vec3 lightColor;

uniform float shadowSplitDistance;

in vec2 out_uv;
out vec4 fragColor;

const vec3 FOG_COLOR = vec3(0.6, 0.7, 0.8);
const float FOG_START = 110.0;
const float FOG_END = 120.0;

const vec3 SUN_COLOR = vec3(1.0, 0.95, 0.85);
const float SUN_INTENSITY = 512.0;

vec2 poissonDisk[64] = vec2[](
    vec2(-0.942, -0.399), vec2(0.945, -0.768), vec2(-0.094, -0.929), vec2(0.344, 0.293),
    vec2(-0.815, 0.457), vec2(-0.815, -0.879), vec2(-0.382, 0.276), vec2(0.974, 0.756),
    vec2(0.443, -0.975), vec2(0.537, -0.473), vec2(-0.264, -0.418), vec2(0.791, 0.190),
    vec2(-0.241, 0.997), vec2(-0.814, 0.914), vec2(0.199, 0.786), vec2(0.143, -0.141),
    vec2(-0.512, 0.623), vec2(0.678, 0.432), vec2(-0.321, -0.756), vec2(0.876, -0.234),
    vec2(-0.987, 0.123), vec2(0.432, 0.876), vec2(-0.654, -0.543), vec2(0.234, -0.987),
    vec2(-0.123, 0.765), vec2(0.765, -0.432), vec2(-0.876, -0.123), vec2(0.543, 0.654),
    vec2(-0.432, 0.987), vec2(0.321, -0.678), vec2(-0.678, 0.876), vec2(0.987, -0.321),
    vec2(0.234, 0.567), vec2(-0.345, 0.456), vec2(0.654, -0.123), vec2(-0.876, 0.234),
    vec2(0.111, -0.555), vec2(-0.555, -0.111), vec2(0.789, 0.456), vec2(-0.123, -0.789),
    vec2(0.456, -0.654), vec2(-0.789, 0.123), vec2(0.321, 0.876), vec2(-0.654, 0.321),
    vec2(0.555, -0.789), vec2(-0.987, -0.456), vec2(0.876, 0.555), vec2(-0.456, -0.987),
    vec2(0.123, -0.321), vec2(-0.321, -0.654), vec2(0.654, 0.789), vec2(-0.789, 0.555),
    vec2(0.456, -0.876), vec2(-0.111, 0.987), vec2(0.987, -0.234), vec2(-0.234, -0.555),
    vec2(0.345, 0.789), vec2(-0.567, -0.234), vec2(0.789, -0.567), vec2(-0.876, 0.789),
    vec2(0.555, 0.111), vec2(-0.432, -0.321), vec2(0.678, -0.456), vec2(-0.345, 0.123)
);

vec3 fresnelSchlick(float cosTheta, vec3 F0);
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

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

    if (proj.z > 1.0 || proj.z < 0.0)
        return 0.0;

    float currentDepth = proj.z;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    mat2 rot = getRotation(uv);

    vec3 normal = normalize(texture(gNormal, uv).rgb * 2.0 - 1.0);
    
    float NdotL = dot(normal, normalize(lightDir1));
    
    float bias;
    if (radiusMultiplier > 0.8) {
        bias = 0.0001 + 0.0003 * (1.0 - NdotL) + texelSize.x * 0.5;
    } else {
        bias = 0.001 + 0.002 * (1.0 - NdotL) + texelSize.x * 3.0;
    }

    float shadow = 0.0;
    float radius = 2.0 * radiusMultiplier;
    int samples = (radiusMultiplier == 0.5) ? 16 : 32;

    for (int i = 0; i < samples; i++)
    {
        vec2 offset = rot * poissonDisk[i] * texelSize * radius;
        float closestDepth = texture(shadowMap, proj.xy + offset).r;
        shadow += step(closestDepth + bias, currentDepth);
    }

    return shadow / float(samples);
}

float calculateShadow(vec3 worldPos, vec2 uv)
{
    float dist = length(worldPos - getCameraPos());
    
    float blendStart = shadowSplitDistance - 16.0;
    float blendEnd = shadowSplitDistance + 16.0;
    
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
    float ao = texture(dSSAO, out_uv).r;
    vec3 albedo = texture(gAlbedo, out_uv).rgb;
    float depth = texture(gDepth, out_uv).r;
    vec3 normal = normalize(texture(gNormal, out_uv).rgb * 2.0 - 1.0);
    float roughness = texture(dSSR, out_uv).g;
    float metallic = texture(dSSR, out_uv).b;

    float waterDepth = texture(gWaterDepth, out_uv).r;
    bool isWater = waterDepth < depth - 0.0001;

    if (isWater) {
        albedo = texture(gWaterAlbedo, out_uv).rgb;
        normal = normalize(texture(gWaterNormal, out_uv).rgb * 2.0 - 1.0);
        roughness = texture(gWaterRoughness, out_uv).x;
        metallic = texture(gWaterRoughness, out_uv).y;
        depth = waterDepth;
    }

    bool isSky = depth > 0.9999;

    vec3 worldPos = reconstructWorldPosition(depth);
    vec3 cameraPos = getCameraPos();
    vec3 viewDir = normalize(cameraPos - worldPos);
    
    float shadow = calculateShadow(worldPos, out_uv);

    float distToCamera = length(worldPos - cameraPos);
    vec3 lightDir = normalize(mix(lightDir1, lightDir2, 
        clamp((distToCamera - (shadowSplitDistance - 8.0)) / 16.0, 0.0, 1.0)));

    vec3 V = viewDir;
    vec3 L = lightDir;
    vec3 H = normalize(L + V);

    float NdotL = max(dot(normal, L), 0.0);
    float NdotV = max(dot(normal, V), 0.0);

    vec4 ssr1 = texture(dSSR, out_uv);
    vec4 ssr2 = texture(dSSRWater, out_uv);

    vec3 ssrColor;
    float ssrAlpha;

    if (isWater) {
        ssrColor = ssr2.rgb;
        ssrAlpha = ssr2.a;
    } else {
        ssrColor = ssr1.rgb;
        ssrAlpha = ssr1.a;
    }

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(NdotV, F0);

    vec3 ambient = albedo * ao * 0.25;

    vec3 diffuse = albedo * lightColor * NdotL * (1.0 - shadow);

    vec3 directLighting = ambient + diffuse;

    vec3 finalColor;
    if (!isSky)
        finalColor = directLighting + ssrColor * ssrAlpha;
    else
        finalColor = directLighting;

    float dist = length(worldPos - cameraPos);
    float fogFactor = clamp((FOG_END - dist) / (FOG_END - FOG_START), 0.0, 1.0);

    vec3 rd = getRd(worldPos, cameraPos);
    vec3 sunColor = getSun(rd, lightDir);

    float fresnel = max(max(F.r, F.g), F.b);

    finalColor =
        directLighting +
        ssrColor * ssrAlpha * fresnel;

    if (isSky) {
        finalColor += sunColor;
    }

    fragColor = vec4(finalColor, 1.0);
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
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}