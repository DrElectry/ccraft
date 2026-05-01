#version 330 core

#define SAMPLES 32
#define BLUR_STRENGTH 0.0125
#define SKY_DEPTH_THRESHOLD 0.9995

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform sampler2D velocityTexture;
uniform sampler2D depthTexture;

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    float depth = texture(depthTexture, out_uv).r;

    // no motion blur on sky
    if (depth >= 1.0) {
        fragColor = vec4(texture(colorTexture, out_uv).rgb, 1.0);
        return;
    }

    vec2 velocity = texture(velocityTexture, out_uv).xy;
    velocity *= BLUR_STRENGTH;

    float velMag = length(velocity);
    if (velMag < 0.0001) {
        fragColor = vec4(texture(colorTexture, out_uv).rgb, 1.0);
        return;
    }

    float maxBlur = 0.08;
    if (velMag > maxBlur) {
        velocity = normalize(velocity) * maxBlur;
    }

    // Add noise offset to break up patterns
    float noise = rand(out_uv * 100.0) * 0.5;
    
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < SAMPLES; i++)
    {
        // Center-biased distribution with noise offset
        float t = (float(i) + noise) / float(SAMPLES - 1) - 0.5;
        
        // Apply smooth falloff - weights stronger in center, weaker at edges
        float weight = 1.0 - abs(t * 2.0);
        weight = weight * weight;
        
        vec2 uv = out_uv - velocity * t;

        color += texture(colorTexture, uv).rgb * weight;
        totalWeight += weight;
    }

    color /= totalWeight;

    fragColor = vec4(color, 1.0);
}
