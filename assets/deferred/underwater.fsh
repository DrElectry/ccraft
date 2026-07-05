#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform int samples = 20;
uniform float time;

uniform sampler2D dirt;

void main() {
    vec2 center = vec2(0.5 + cos(time) * 0.05, 0.5 + sin(time * 1.3) * 0.05);
    float strength = abs(sin(time))*0.025;

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