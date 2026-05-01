#version 330 core

in vec2 out_uv;

out vec4 fragColor;

uniform sampler2D sceneTex;

void main()
{
    vec3 color = texture(sceneTex, out_uv).rgb;

    float brightness =
        dot(color, vec3(0.2126, 0.7152, 0.0722));

    if (brightness > 0.8)
        fragColor = vec4(color, 1.0);
    else
        fragColor = vec4(0.0);
}