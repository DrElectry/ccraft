#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D colorTexture;
uniform sampler2D bloomTexture;

void main()
{
    vec3 baseColor = texture(colorTexture, out_uv).rgb;

    vec3 bloom = texture(bloomTexture, out_uv).rgb;
    bloom = bloom / (1.0 + bloom);

    vec3 col = baseColor + bloom * 0.35;

    float d = distance(out_uv, vec2(0.5));
    float vignette = 1.0 - d * d * 1.8;
    vignette = clamp(vignette, 0.0, 1.0);

    fragColor = vec4(col * vignette, 1.0);
}