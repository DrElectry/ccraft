#version 330 core

in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D tex_atlas;
uniform uint color_data;

vec3 vga_color(uint i) {
    i = i & 15u;

    if (i == 0u) return vec3(0.0);
    if (i == 1u) return vec3(0.0, 0.0, 170.0/255.0);
    if (i == 2u) return vec3(0.0, 170.0/255.0, 0.0);
    if (i == 3u) return vec3(0.0, 170.0/255.0, 170.0/255.0);
    if (i == 4u) return vec3(170.0/255.0, 0.0, 0.0);
    if (i == 5u) return vec3(170.0/255.0, 0.0, 170.0/255.0);
    if (i == 6u) return vec3(170.0/255.0, 85.0/255.0, 0.0);
    if (i == 7u) return vec3(170.0/255.0, 170.0/255.0, 170.0/255.0);

    if (i == 8u) return vec3(85.0/255.0);
    if (i == 9u) return vec3(85.0/255.0, 85.0/255.0, 1.0);
    if (i == 10u) return vec3(85.0/255.0, 1.0, 85.0/255.0);
    if (i == 11u) return vec3(85.0/255.0, 1.0, 1.0);
    if (i == 12u) return vec3(1.0, 85.0/255.0, 85.0/255.0);
    if (i == 13u) return vec3(1.0, 85.0/255.0, 1.0);
    if (i == 14u) return vec3(1.0, 1.0, 85.0/255.0);
    if (i == 15u) return vec3(1.0);

    return vec3(1.0);
}

void main() {
    uint packed_data = color_data;

    uint fg = packed_data & 0x0Fu;
    uint bg = (packed_data >> 4) & 0x0Fu;

    vec4 sample_color = texture(tex_atlas, v_uv);

    vec3 color = vga_color(fg);

    if (sample_color.rgb != vec3(1.0))
       color = vga_color(bg);

    frag_color = vec4(color, 1.0);
}