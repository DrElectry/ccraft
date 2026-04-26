#version 330 core
in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D tex_atlas;

void main() {
    vec4 sample_color = texture(tex_atlas, v_uv);
    if (sample_color.rgb != vec3(1.0f, 1.0f, 1.0f))
        discard;
    frag_color = sample_color;
}

