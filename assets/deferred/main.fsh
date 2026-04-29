#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D dShadow;

uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform mat4 light_space_matrix;

in vec2 out_uv;
out vec4 fragColor;

float shadowFactor(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0 || projCoords.x > 1.0 || projCoords.x < 0.0 || 
        projCoords.y > 1.0 || projCoords.y < 0.0)
        return 0.0;
        
    float closestDepth = texture(dShadow, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    float shadow = currentDepth > closestDepth + bias ? 1.0 : 0.0;
    return shadow;
}


void main()
{
    vec3 normal = texture(gNormal, out_uv).rgb;
    float depth = texture(gDepth, out_uv).r;
    vec4 ndc = vec4(out_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = inv_projection * ndc;
    viewPos /= viewPos.w;

    vec4 worldPos = inv_view * viewPos;
    vec4 fragPosLightSpace = light_space_matrix * worldPos;
    vec3 color = texture(gAlbedo, out_uv).rgb;
    float shadow = shadowFactor(fragPosLightSpace);

    color *= (1.0 - shadow);

    color+=normal*0.001;

    fragColor = vec4(color, 1.0);
}