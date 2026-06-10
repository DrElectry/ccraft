#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform sampler2D depthTexture;
uniform sampler2D bloomTexture;

void main()
{
    float aperture = 0.10;
    float blurIntensity = 16.0;

    float nearFocus = 0.985;
    float farFocus  = 0.99;

    vec2 texel = 1.0 / vec2(textureSize(colorTexture, 0));

    float depth = texture(depthTexture, out_uv).r;

    float cocNear = smoothstep(nearFocus, 0.0, depth);
    float cocFar  = smoothstep(farFocus, 1.0, depth);
    float coc = max(cocNear, cocFar);
    coc = clamp(coc, 0.0, 1.0);

    float radius = coc * aperture * blurIntensity;

    vec3 col = texture(colorTexture, out_uv).rgb;

    vec2 offsets[17];
    offsets[0]  = vec2( 0.0000,  0.0000);
    offsets[1]  = vec2( 0.3260,  0.4050);
    offsets[2]  = vec2(-0.3260,  0.4050);
    offsets[3]  = vec2( 0.8410,  0.1440);
    offsets[4]  = vec2(-0.8410,  0.1440);
    offsets[5]  = vec2( 0.2900, -0.7890);
    offsets[6]  = vec2(-0.2900, -0.7890);
    offsets[7]  = vec2( 0.9350, -0.3530);
    offsets[8]  = vec2(-0.9350, -0.3530);
    offsets[9]  = vec2( 0.1500,  0.9800);
    offsets[10] = vec2(-0.1500,  0.9800);
    offsets[11] = vec2( 0.0000, -0.5000);
    offsets[12] = vec2( 0.7000,  0.7000);
    offsets[13] = vec2(-0.7000,  0.7000);
    offsets[14] = vec2( 0.7000, -0.7000);
    offsets[15] = vec2(-0.7000, -0.7000);
    offsets[16] = vec2( 0.0000,  0.9800);

    float weights[17];
    weights[0]  = 0.12;
    weights[1]  = 0.08;
    weights[2]  = 0.08;
    weights[3]  = 0.07;
    weights[4]  = 0.07;
    weights[5]  = 0.06;
    weights[6]  = 0.06;
    weights[7]  = 0.05;
    weights[8]  = 0.05;
    weights[9]  = 0.05;
    weights[10] = 0.05;
    weights[11] = 0.05;
    weights[12] = 0.04;
    weights[13] = 0.04;
    weights[14] = 0.04;
    weights[15] = 0.04;
    weights[16] = 0.05;

    vec3 sum = vec3(0.0);

    for (int i = 0; i < 17; i++)
    {
        vec2 offset = offsets[i] * radius * texel;
        sum += texture(colorTexture, out_uv + offset).rgb * weights[i];
    }

    vec3 bloom = texture(bloomTexture, out_uv).rgb;
    bloom = bloom / (1.0 + bloom);

    vec3 finalCol = sum + bloom * 0.55;

    float vignette = 1.0 - pow(distance(out_uv, vec2(0.5)), 2.0) * 1.25;
    vignette = clamp(vignette, 0.0, 1.0);

    fragColor = vec4(finalCol * vignette, 1.0);
}