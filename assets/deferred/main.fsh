#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gDepth;

in vec2 out_uv;
out vec4 fragColor;

void main()
{
    vec3 data = texture(gAlbedo, out_uv).rgb;
    vec3 normal = texture(gNormal, out_uv).rgb;
    float depth = texture(gDepth, out_uv).r;

    fragColor = vec4(
        data + normal * 0.00001 + vec3(depth) * 0.00001,
        1.0
    ); // nice and useful deferred rtx dlss 5 vulkan metal dx11 and friends shader
}