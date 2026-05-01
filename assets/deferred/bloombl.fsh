#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D image;
uniform vec2 direction;

const float blurScale = 4.0;

void main()
{
    vec2 texel = (1.0 / textureSize(image, 0)) * blurScale;

    vec3 result = vec3(0.0);

    float w0 = 0.1964825501511404;
    float w1 = 0.1760326633821498;
    float w2 = 0.1217031289285933;
    float w3 = 0.0648251870288647;
    float w4 = 0.0276716409147871;
    float w5 = 0.0091937996114339;
    float w6 = 0.0022166234893290;

    result += texture(image, out_uv).rgb * w0;

    vec2 offset;

    offset = direction * texel * 1.0;
    result += texture(image, out_uv + offset).rgb * w1;
    result += texture(image, out_uv - offset).rgb * w1;

    offset = direction * texel * 2.0;
    result += texture(image, out_uv + offset).rgb * w2;
    result += texture(image, out_uv - offset).rgb * w2;

    offset = direction * texel * 3.0;
    result += texture(image, out_uv + offset).rgb * w3;
    result += texture(image, out_uv - offset).rgb * w3;

    offset = direction * texel * 4.0;
    result += texture(image, out_uv + offset).rgb * w4;
    result += texture(image, out_uv - offset).rgb * w4;

    offset = direction * texel * 5.0;
    result += texture(image, out_uv + offset).rgb * w5;
    result += texture(image, out_uv - offset).rgb * w5;

    offset = direction * texel * 6.0;
    result += texture(image, out_uv + offset).rgb * w6;
    result += texture(image, out_uv - offset).rgb * w6;

    fragColor = vec4(result, 1.0);
}