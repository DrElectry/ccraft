#version 330 core

uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gDepth;

in vec2 out_uv;
out vec4 fragColor;

void main()
{
    vec3 color;

    if (out_uv.x < 0.3333)
    {
        color = texture(gAlbedo, out_uv).rgb;
    }
    else if (out_uv.x < 0.6666)
    {
        vec3 n = texture(gNormal, out_uv).rgb;
        color = n * 0.5 + 0.5;
    }
    else
    {
        float d = texture(gDepth, out_uv).r;
        color = vec3(d);
    }

    fragColor = vec4(color, 1.0);
}