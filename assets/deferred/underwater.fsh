#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform float strength = 0.01;
uniform int samples = 20;
uniform float time;

void main() {
    vec2 center = vec2(0.5 + cos(time) * 0.05, 0.5 + sin(time * 1.3) * 0.05);

    float bulgeRadius = 1.0;
    float bulgeStrength = -0.15;

    vec2 uv = out_uv;
    vec2 dirToCenter = uv - center;
    float distToCenter = length(dirToCenter);
    
    vec2 bulgedUv = uv;
    if (distToCenter < bulgeRadius) {
        float norm = distToCenter / bulgeRadius;
        float factor = 1.0 + bulgeStrength * (1.0 - norm * norm);
        bulgedUv = center + dirToCenter * factor;
    }
    
    vec4 originalColor = texture(colorTexture, bulgedUv);
    
    vec2 dir = bulgedUv - center;
    float dist = length(dir);
    if (dist < 0.0001) {
        fragColor = originalColor;
        return;
    }
    dir /= dist;
    
    float maxBlurDist = dist * strength;
    vec4 blurred = vec4(0.0);
    for (int i = 0; i < samples; ++i) {
        float t = (float(i) / float(samples - 1) - 0.5) * 2.0;
        vec2 sampleUV = bulgedUv + dir * t * maxBlurDist;
        blurred += texture(colorTexture, sampleUV);
    }
    blurred /= float(samples);
    
    float blurFactor = smoothstep(0.0, 1.0, dist * 1.5);
    fragColor = mix(originalColor, blurred, blurFactor);
}