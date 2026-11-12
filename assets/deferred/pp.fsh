#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform sampler2D depthTexture;
uniform sampler2D bloomTexture;

uniform float blurStrength;
uniform float focusDistance;
uniform float time;

const float NEAR_PLANE   = 0.1;
const float FAR_PLANE    = 100.0;
const float APERTURE     = 0.08;

const int   SAMPLES      = 16;
const float GOLDEN_ANGLE = 2.39996323;
const float TAU          = 6.28318531;

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * NEAR_PLANE * FAR_PLANE) /
           (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));
}

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float signedCoC(float linDepth) {
    float coc = (linDepth - focusDistance) / max(focusDistance, 1e-4);
    return clamp(coc * APERTURE, -1.0, 1.0);
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(colorTexture, 0));

    float depth     = texture(depthTexture, out_uv).r;
    float linDepth  = linearizeDepth(depth);
    float cocCenter = abs(signedCoC(linDepth));
    float radius    = cocCenter * blurStrength;

    vec3 col = texture(colorTexture, out_uv).rgb;

    if (radius > 0.5) {
        float rot = hash21(gl_FragCoord.xy + time * 60.0) * TAU;

        vec3  sum         = col;
        float totalWeight = 1.0;

        for (int i = 0; i < SAMPLES; ++i) {
            float fi    = float(i);
            float r     = sqrt((fi + 0.5) / float(SAMPLES));
            float theta = fi * GOLDEN_ANGLE + rot;

            vec2 offset = vec2(cos(theta), sin(theta)) * r * radius * texel;
            vec2 uv     = out_uv + offset;

            vec3  sCol = texture(colorTexture, uv).rgb;
            float sLin = linearizeDepth(texture(depthTexture, uv).r);
            float sCoC = abs(signedCoC(sLin));

            float w = 1.0;
            if (sLin > linDepth)
                w = clamp((cocCenter + 1e-3) / (sCoC + 1e-3), 0.0, 1.0);

            w *= 1.0 - smoothstep(0.75, 1.0, r);

            sum         += sCol * w;
            totalWeight += w;
        }
        col = sum / totalWeight;
    }

    vec3 bloom = texture(bloomTexture, out_uv).rgb;
    bloom = bloom / (1.0 + bloom);
    col  += bloom * 0.55;

    float vig = 1.0 - pow(distance(out_uv, vec2(0.5)), 4.0) * 2.5;
    vig = clamp(vig, 0.0, 1.0);

    fragColor = vec4(col * vig, 1.0);
}