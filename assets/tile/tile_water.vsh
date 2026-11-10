#version 330 core

layout(location = 0) in uvec2 packed_face;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform float time;

out vec2 out_uv;
out vec3 out_world_pos;
out vec3 out_normal;
out float out_light;

void unpack_face(uint face, out vec3 q0, out vec3 q1, out vec3 q2, out vec3 q3,
                 out vec3 N, out int reverse)
{
    reverse = 0;

    if (face == 0u) {
        q0 = vec3(0.0, 0.0, 1.0); q1 = vec3(1.0, 0.0, 1.0);
        q2 = vec3(1.0, 1.0, 1.0); q3 = vec3(0.0, 1.0, 1.0);
        N = vec3(0.0, 0.0, 1.0);
    } else if (face == 1u) {
        q0 = vec3(1.0, 0.0, 0.0); q1 = vec3(0.0, 0.0, 0.0);
        q2 = vec3(0.0, 1.0, 0.0); q3 = vec3(1.0, 1.0, 0.0);
        N = vec3(0.0, 0.0, -1.0);
    } else if (face == 2u) {
        q0 = vec3(0.0, 0.0, 0.0); q1 = vec3(0.0, 0.0, 1.0);
        q2 = vec3(0.0, 1.0, 1.0); q3 = vec3(0.0, 1.0, 0.0);
        N = vec3(-1.0, 0.0, 0.0);
    } else if (face == 3u) {
        q0 = vec3(1.0, 0.0, 1.0); q1 = vec3(1.0, 0.0, 0.0);
        q2 = vec3(1.0, 1.0, 0.0); q3 = vec3(1.0, 1.0, 1.0);
        N = vec3(1.0, 0.0, 0.0);
    } else if (face == 4u) {
        q0 = vec3(0.0, 1.0, 1.0); q1 = vec3(1.0, 1.0, 1.0);
        q2 = vec3(1.0, 1.0, 0.0); q3 = vec3(0.0, 1.0, 0.0);
        N = vec3(0.0, 1.0, 0.0);
    } else {
        q0 = vec3(0.0, 0.0, 0.0); q1 = vec3(1.0, 0.0, 0.0);
        q2 = vec3(1.0, 0.0, 1.0); q3 = vec3(0.0, 0.0, 1.0);
        N = vec3(0.0, -1.0, 0.0);
    }
}

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x)
         + (c - a) * u.y * (1.0 - u.x)
         + (d - b) * u.x * u.y;
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amp = 0.5;
    float freq = 1.0;

    for (int i = 0; i < 2; i++)
    {
        value += noise(p * freq) * amp;
        freq *= 2.0;
        amp *= 0.5;
    }

    return value;
}

float wave(vec3 p)
{
    vec2 uv1 = p.xz * 0.7 + vec2(1.0, 0.3) * time * 0.5;
    vec2 uv2 = p.xz * 0.8 + vec2(-0.3, 1.0) * time * 0.4;

    float n = fbm(uv1) * 0.6 + fbm(uv2) * 0.4;

    return n * 0.08;
}

void main()
{
    uint a = packed_face.x;
    uint b = packed_face.y;

    uint bx = a & 15u;
    uint bz = (a >> 4u) & 15u;
    uint by = (a >> 8u) & 255u;
    uint face = (a >> 16u) & 15u;
    uint atlas = (a >> 20u) & 255u;
    uint ao_flip = (a >> 28u) & 1u;

    float lights[4];
    lights[0] = float(b & 63u) / 63.0;
    lights[1] = float((b >> 6u) & 63u) / 63.0;
    lights[2] = float((b >> 12u) & 63u) / 63.0;
    lights[3] = float((b >> 18u) & 63u) / 63.0;

    vec3 q0, q1, q2, q3, N;
    int reverse;
    unpack_face(face, q0, q1, q2, q3, N, reverse);

    int vid = gl_VertexID;
    int q;
    if (reverse != 0) {
        int map[6] = int[](0, 2, 1, 2, 0, 3);
        q = map[vid];
    } else if (ao_flip != 0u) {
        int map[6] = int[](0, 1, 3, 1, 2, 3);
        q = map[vid];
    } else {
        int map[6] = int[](0, 1, 2, 2, 3, 0);
        q = map[vid];
    }

    vec3 local = (q == 0) ? q0 : (q == 1) ? q1 : (q == 2) ? q2 : q3;
    vec3 in_vert = local + vec3(float(bx), float(by), float(bz));

    float tile_size = 1.0 / 16.0;
    float col = float(atlas % 16u);
    float row = float(atlas / 16u);
    float u0 = col * tile_size;
    float v0 = row * tile_size;
    float u1 = u0 + tile_size;
    float v1 = v0 + tile_size;
    vec2 uvs[4] = vec2[](
        vec2(u0, v1),
        vec2(u1, v1),
        vec2(u1, v0),
        vec2(u0, v0)
    );

    out_uv = uvs[q];
    out_light = lights[q];

    vec3 original_normal = normalize((model * vec4(N, 0.0)).xyz);

    vec3 world_pos = (model * vec4(in_vert, 1.0)).xyz;
    world_pos -= original_normal * 0.001;

    vec3 displaced = world_pos;

    out_normal = original_normal;

    if (dot(original_normal, vec3(0.0, 1.0, 0.0)) > 0.999)
    {
        float eps = 0.001;

        float h  = wave(world_pos);
        float hx = wave(world_pos + vec3(eps, 0.0, 0.0));
        float hz = wave(world_pos + vec3(0.0, 0.0, eps));

        displaced.y += h;
        displaced.y -= 0.1;

        vec2 noise_uv = world_pos.xz * 0.5 + vec2(1.7, 3.2) * time * 0.3;
        displaced.y += fbm(noise_uv) * 0.1;

        vec3 tangent = normalize(vec3(eps, hx - h, 0.0));
        vec3 bitangent = normalize(vec3(0.0, hz - h, eps));
        vec3 normal = normalize(cross(bitangent, tangent));

        out_normal = normalize((model * vec4(normal, 0.0)).xyz);
    }

    out_world_pos = displaced;
    gl_Position = proj * view * vec4(displaced, 1.0);
}
