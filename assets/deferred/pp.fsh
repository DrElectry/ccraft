#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform sampler2D depthTexture;
uniform sampler2D bloomTexture;

void main()
{
    float aperture = 0.10;
    float blurIntensity = 45.0;

    float nearFocus = 0.95;
    float farFocus  = 0.99;

    vec2 texel = 1.0 / vec2(textureSize(colorTexture, 0));

    float depth = texture(depthTexture, out_uv).r;

    float cocNear = smoothstep(nearFocus, 0.0, depth);
    float cocFar  = smoothstep(farFocus, 1.0, depth);
    float coc = max(cocNear, cocFar);
    coc = clamp(coc, 0.0, 1.0);

    float radius = coc * aperture * blurIntensity;

    vec3 col = texture(colorTexture, out_uv).rgb;

    vec2 offsets[12];
    offsets[0]  = vec2( 0.0,  0.0);
    offsets[1]  = vec2( 0.326,  0.405);
    offsets[2]  = vec2(-0.326,  0.405);
    offsets[3]  = vec2( 0.841,  0.144);
    offsets[4]  = vec2(-0.841,  0.144);
    offsets[5]  = vec2( 0.290, -0.789);
    offsets[6]  = vec2(-0.290, -0.789);
    offsets[7]  = vec2( 0.935, -0.353);
    offsets[8]  = vec2(-0.935, -0.353);
    offsets[9]  = vec2( 0.150,  0.980);
    offsets[10] = vec2(-0.150,  0.980);
    offsets[11] = vec2( 0.000, -0.500);

    float weights[12];
    weights[0]  = 0.18;
    weights[1]  = 0.10;
    weights[2]  = 0.10;
    weights[3]  = 0.08;
    weights[4]  = 0.08;
    weights[5]  = 0.07;
    weights[6]  = 0.07;
    weights[7]  = 0.05;
    weights[8]  = 0.05;
    weights[9]  = 0.06;
    weights[10] = 0.06;
    weights[11] = 0.06;

    vec3 sum = vec3(0.0);

    for (int i = 0; i < 12; i++)
    {
        vec2 offset = offsets[i] * radius * texel;
        sum += texture(colorTexture, out_uv + offset).rgb * weights[i];
    }

    vec3 bloom = texture(bloomTexture, out_uv).rgb;
    bloom = bloom / (1.0 + bloom);

    vec3 finalCol = sum + bloom * 0.55;

    float vignette = 1.0 - pow(distance(out_uv, vec2(0.5)), 2.0) * 1.5;
    vignette = clamp(vignette, 0.0, 1.0);

    fragColor = vec4(finalCol * vignette, 1.0);
}