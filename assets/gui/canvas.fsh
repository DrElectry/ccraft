#version 330 core

in vec2 out_uv;

out vec4 fragColor;

uniform sampler2D tex;
uniform float alpha;
uniform float radius; // radius in pixels
uniform vec2 size;    // rectangle size in pixels

void main() {
    vec4 data = texture(tex, out_uv);

    if (data.rgb == vec3(1.0, 0.0, 1.0)) {
        discard;
    }

    vec2 p = (out_uv - 0.5) * size;

    // clamp radius to half the smallest dimension
    float r = clamp(radius, 0.0, min(size.x, size.y) * 0.5);

    // signed distance to rounded rectangle
    vec2 q = abs(p) - (size * 0.5 - vec2(r));

    float dist = length(max(q, 0.0))
               + min(max(q.x, q.y), 0.0)
               - r;

    // anti aliasing
    float aa = fwidth(dist);
    float mask = 1.0 - smoothstep(-aa, aa, dist);

    fragColor = vec4(data.rgb, alpha * mask);
}