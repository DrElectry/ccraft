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

const vec3 FOG_COLOR = vec3(0.6, 0.7, 0.8);
const float FOG_START = 80.0;
const float FOG_END = 110.0;

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

void main()
{
    float ao = texture(dSSAO, out_uv).r;
    vec3 albedo = texture(gAlbedo, out_uv).rgb;
    float depth = texture(gDepth, out_uv).r;
    vec3 normal = normalize(texture(gNormal, out_uv).rgb * 2.0 - 1.0);
    float roughness = texture(dSSR, out_uv).g;
    float metallic = texture(dSSR, out_uv).b;

    bool isSky = depth > 0.9999;

    vec3 worldPos = reconstructWorldPosition(depth);
    vec4 fragPosLightSpace = light_space_matrix * vec4(worldPos, 1.0);

    float shadow = pcf(fragPosLightSpace);

    vec3 L = normalize(lightDir);
    vec3 V = normalize(-worldPos);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(normal, L), 0.0);
    float NdotV = max(dot(normal, V), 0.0);

    vec3 ssrColor = texture(dSSR, out_uv).rgb;
    float ssrAlpha = texture(dSSR, out_uv).a;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(NdotV, F0);

    vec3 ambient = albedo * ao * 0.5;

    vec3 diffuse = albedo * lightColor * NdotL * (1.0 - shadow);

    vec3 specular = vec3(0.0);
    if (NdotL > 0.0)
    {
        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.001;
        specular = numerator / denominator;
        specular *= lightColor * NdotL * (1.0 - shadow);
    }

    vec3 directLighting = ambient + diffuse + specular;

    vec3 finalColor;
    if (!isSky)
        finalColor = directLighting + ssrColor * ssrAlpha;
    else
        finalColor = directLighting;

    float dist = length(worldPos - getCameraPos());
    float fogFactor = clamp((FOG_END - dist) / (FOG_END - FOG_START), 0.0, 1.0);

    finalColor = mix(FOG_COLOR, finalColor, fogFactor);

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