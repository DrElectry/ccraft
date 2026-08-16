#version 330 core

in vec2 out_uv;

uniform sampler2D frame;
uniform float width;
uniform float height;

out vec4 fragColor;

// reluctantly borrowed this from Veloren game project

#define FXAA_REDUCE_MIN   (1.0 / 256.0)
#define FXAA_REDUCE_MUL   (1.0 / 2.0)
#define FXAA_SPAN_MAX     24.0

void main() {
    vec2 resolution = vec2(width, height);
    vec2 inverseScreenSize = 1.0 / resolution;
    vec2 texCoord = out_uv;

    vec3 rgbNW = texture(frame, texCoord + vec2(-1.0, -1.0) * inverseScreenSize).xyz;
    vec3 rgbNE = texture(frame, texCoord + vec2( 1.0, -1.0) * inverseScreenSize).xyz;
    vec3 rgbSW = texture(frame, texCoord + vec2(-1.0,  1.0) * inverseScreenSize).xyz;
    vec3 rgbSE = texture(frame, texCoord + vec2( 1.0,  1.0) * inverseScreenSize).xyz;
    vec3 rgbM  = texture(frame, texCoord).xyz;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    if (lumaMax - lumaMin > max(FXAA_REDUCE_MIN, lumaMax * FXAA_REDUCE_MUL)) {
        vec2 dir;
        dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
        dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

        float dirReduce = max(
            (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
            FXAA_REDUCE_MIN
        );
        float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

        dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
                  max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
                  dir * rcpDirMin)) * inverseScreenSize;

        vec3 rgbA = 0.5 * (
            texture(frame, texCoord + dir * (1.0/3.0 - 0.5)).xyz +
            texture(frame, texCoord + dir * (2.0/3.0 - 0.5)).xyz
        );
        vec3 rgbB = 0.5 * (
            texture(frame, texCoord + dir * (1.0/3.0 + 0.5)).xyz +
            texture(frame, texCoord + dir * (2.0/3.0 + 0.5)).xyz
        );

        float lumaA = dot(rgbA, luma);
        float lumaB = dot(rgbB, luma);

        if ((lumaA < lumaMin) || (lumaA > lumaMax) ||
            (lumaB < lumaMin) || (lumaB > lumaMax)) {
            fragColor = vec4(rgbM, 1.0);
        } else {
            fragColor = vec4((rgbA + rgbB) * 0.5, 1.0);
        }
    } else {
        fragColor = vec4(rgbM, 1.0);
    }
}