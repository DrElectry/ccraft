#version 330 core

#define SAMPLES 24
#define BLUR_STRENGTH 0.0075
#define MAX_BLUR 0.05

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform sampler2D velocityTexture;
uniform sampler2D depthTexture;
uniform sampler2D bloomTexture;

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
    float depth = texture(depthTexture, out_uv).r;
    vec3 baseColor = texture(colorTexture, out_uv).rgb;

    if (depth >= 1.0)
    {
        vec3 bloom = texture(bloomTexture, out_uv).rgb;
        bloom = bloom / (1.0 + bloom);
        fragColor = vec4(baseColor + bloom * 0.35, 1.0);
        return;
    }

    vec2 velocity = texture(velocityTexture, out_uv).xy;
    velocity *= BLUR_STRENGTH;

    float velMag = length(velocity);

    if (velMag < 0.00001)
    {
        vec3 bloom = texture(bloomTexture, out_uv).rgb;
        bloom = bloom / (1.0 + bloom);
        fragColor = vec4(baseColor + bloom * 0.35, 1.0);
        return;
    }

    if (velMag > MAX_BLUR)
    {
        velocity *= MAX_BLUR / velMag;
    }

    float offset = hash12(gl_FragCoord.xy) - 0.5;

    vec3 color = vec3(0.0);
    float wsum = 0.0;

    for (int i = 0; i < SAMPLES; i++)
    {
        float t = (float(i) / float(SAMPLES - 1)) - 0.5;
        vec2 uv = out_uv - velocity * t;

        float w = 1.0 - abs(t);
        w *= w;

        color += texture(colorTexture, uv).rgb * w;
        wsum += w;
    }

    color /= max(wsum, 0.0001);

    vec3 bloom = texture(bloomTexture, out_uv).rgb;
    bloom = bloom / (1.0 + bloom);

    color += bloom * 0.15;

    fragColor = vec4(color, 1.0);
}