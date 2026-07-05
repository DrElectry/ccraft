#version 330 core

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gViewPosition;
layout(location = 3) out vec2 gRoughness;

in vec2 out_uv;
in vec3 out_world_pos;
in vec3 out_normal;
in float out_light;

uniform sampler2D tex;
uniform sampler2D roug;
uniform float time;
uniform mat4 view;

void main()
{
    float tile_size = 1.0 / 16.0;

    float fps = 0.5;
    float frameCount = 2.0;

    float animTime = time * fps;

    float frame = floor(animTime);
    float t = fract(animTime);

    float currentFrame = mod(frame, frameCount);
    float nextFrame = mod(frame + 1.0, frameCount);

    vec2 uv1 = out_uv + vec2(currentFrame * tile_size, 0.0);
    vec2 uv2 = out_uv + vec2(nextFrame * tile_size, 0.0);

    vec4 frame1 = texture(tex, uv1);
    vec4 frame2 = texture(tex, uv2);

    vec4 data = mix(frame1, frame2, t);

    if (data.a < 0.1) discard;

    float lit = 0.12 + 0.88 * clamp(out_light, 0.0, 1.0);

    if (out_uv.x < tile_size) {
        gAlbedo = vec4(data.rgb * lit, 0.5);
    } else {
        gAlbedo = vec4(data.rgb * 4.0 * lit, 0.5);
    }

    gNormal = normalize(out_normal);

    gRoughness = texture(roug, out_uv).bb;

    vec4 view_pos = view * vec4(out_world_pos, 1.0);
    gViewPosition = vec4(view_pos.xyz, 1.0);
}