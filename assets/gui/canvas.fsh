#version 330 core

in vec2 out_uv;

out vec4 fragColor;

uniform sampler2D tex;
uniform float alpha;

void main() {
    vec4 data = texture(tex, out_uv);
    if (data.rgb == vec3(1.0f, 0.0f, 1.0f)) {
        discard;
    } else {
        fragColor = vec4(data.rgb, alpha);
    }
}