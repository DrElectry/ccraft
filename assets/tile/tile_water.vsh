#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform float time;

out vec2 out_uv;
out vec3 out_normal;
out vec3 out_view_pos;
out vec3 out_pos;

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

    for(int i = 0; i < 4; i++)
    {
        value += noise(p) * amp;

        p *= 2.0;
        amp *= 0.5;
    }

    return value;
}

float wave(vec3 p)
{
    vec2 uv = p.xz * 0.8;

    uv += vec2(time * 0.15, time * 0.1);

    float n = fbm(uv);

    n += sin(p.x * 3.0 + time * 2.0) * 0.03;
    n += sin(p.z * 4.0 + time * 1.7) * 0.02;

    return n * 0.08;
}

void main()
{
    out_uv = in_uv;

    vec3 world_pos = (model * vec4(in_vert, 1.0)).xyz;

    float h = wave(world_pos);
    float eps = 0.001;

    float hx = wave(world_pos + vec3(eps, 0.0, 0.0));
    float hz = wave(world_pos + vec3(0.0, 0.0, eps));

    float wave_height = h - 0.1;

    vec3 displaced = world_pos;
    displaced.y += wave_height;

    vec3 dx = vec3(eps, hx - h, 0.0);
    vec3 dz = vec3(0.0, hz - h, eps);

    vec3 normal_world = normalize(cross(dz, dx));

    out_normal = normal_world;

    vec4 view_pos = view * vec4(displaced, 1.0);
    out_view_pos = view_pos.xyz;

    out_pos = displaced;

    gl_Position = proj * view_pos;
}