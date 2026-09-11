#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform int samples = 20;
uniform float time;

uniform sampler2D dirt;

vec2 applyDistortions(vec2 uv, vec2 center, float t) {
    vec2 p = uv;
    p.x += sin(uv.y * 8.0 + t * 1.5) * 0.012;
    p.y += cos(uv.x * 8.0 + t * 1.65) * 0.012;
    p.x += cos((uv.x + uv.y) * 5.0 - t * 0.9) * 0.008;
    p.y += sin((uv.x - uv.y) * 5.0 + t * 1.17) * 0.008;
    vec2 dir = uv - center;
    float dist = length(dir);
    if (dist > 0.0001) {
        vec2 ndir = dir / dist;
        float ripple = sin(dist * 12.0 - t * 2.0) * 0.010;
        float falloff = exp(-dist * 3.0);
        p += ndir * ripple * falloff;
    }
    return p;
}

void main() {
    vec2 center = vec2(0.5 + cos(time) * 0.05, 0.5 + sin(time * 1.3) * 0.05);
    float strength = abs(sin(time))*0.025;

    float bulgeRadius = 1.0;
    float bulgeStrength = -0.25;

    vec2 uv = applyDistortions(out_uv, center, time);

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
        vec4 dirta = texture(dirt, out_uv);
        float dirtIntensity = dot(dirta.rgb, vec3(0.299, 0.587, 0.114));
        if (dirtIntensity > 0.01) {
            vec3 finalColor = mix(originalColor.rgb, dirta.rgb, dirtIntensity * 0.5);
            fragColor = vec4(finalColor, 1.0);
        } else {
            fragColor = vec4(originalColor.rgb, 1.0);
        }
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

    vec3 col = mix(originalColor, blurred, blurFactor).rgb;

    vec4 dirta = texture(dirt, bulgedUv);
    float dirtIntensity = dot(dirta.rgb, vec3(0.299, 0.587, 0.114));
    if (dirtIntensity > 0.01) {
        vec3 finalColor = mix(col, dirta.rgb, dirtIntensity * 0.5);
        fragColor = vec4(finalColor, 1.0);
    } else {
        fragColor = vec4(col, 1.0);
    }
}