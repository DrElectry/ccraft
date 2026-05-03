#version 330 core
layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gViewPosition;
layout(location = 3) out vec2 gRoughness;

in vec2 out_uv;
in vec3 out_normal;
in vec3 out_view_pos;
in vec3 out_pos;

uniform sampler2D tex;
uniform sampler2D roug;
uniform float time;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    mat2 rot = mat2(1.6, 1.2, -1.2, 1.6);
    for (int i = 0; i < 2; i++) {
        value += noise(p) * amplitude;
        p = rot * p * 2.0;
        amplitude *= 0.5;
    }
    return value;
}

float sharpFbm(vec2 p) {
    return pow(fbm(p), 2.8);
}

float ridgedFbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    mat2 rot = mat2(1.6, 1.2, -1.2, 1.6);
    for (int i = 0; i < 2; i++) {
        float n = noise(p);
        n = 1.0 - abs(n * 2.0 - 1.0);
        value += n * amplitude;
        p = rot * p * 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
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
    gAlbedo = data.rgb;

    vec3 normal  = normalize(out_normal);
    vec2 worldP  = vec2(out_pos.x + out_pos.y, out_pos.z + out_pos.y) * 0.35;

    vec2 warp = vec2(
        fbm(worldP * 0.005 + time * 0.025),
        fbm(worldP * 0.005 - time * 0.025)
    );
    worldP += warp * 2.0;

    float nx = sharpFbm(worldP * 4.0 + vec2(time * 0.04, 0.0)) * 0.7
             + ridgedFbm(worldP * 9.0 + vec2(0.0,        time * 0.02)) * 0.3;
    float ny = sharpFbm(worldP * 4.0 + vec2(0.0,        time * 0.04)) * 0.7
             + ridgedFbm(worldP * 9.0 + vec2(time * 0.02, 0.0))        * 0.3;

    vec3 perturb = normalize(vec3(
        (nx * 2.0 - 1.0) * 0.85,
        (ny * 2.0 - 1.0) * 0.85,
        1.0
    ));

    gNormal       = normalize(mix(normal, perturb, 0.45));
    gRoughness    = texture(roug, out_uv).bb;
    gViewPosition = vec4(out_view_pos, 1.0);
}