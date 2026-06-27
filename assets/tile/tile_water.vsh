#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_light;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform float time;

out vec2 out_uv;
out vec3 out_world_pos;
out vec3 out_normal;
out float out_light;

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
    out_uv = in_uv;
    out_light = in_light;

    vec3 original_normal = normalize((model * vec4(in_normal, 0.0)).xyz);

    vec3 world_pos = (model * vec4(in_vert, 1.0)).xyz;
    world_pos -= original_normal * 0.01;

    vec3 displaced = world_pos;

    out_normal = original_normal;

    if (dot(original_normal, vec3(0.0, 1.0, 0.0)) > 0.999)
    {
        float eps = 0.00001;

        float h  = wave(world_pos);
        float hx = wave(world_pos + vec3(eps, 0.0, 0.0));
        float hz = wave(world_pos + vec3(0.0, 0.0, eps));

        displaced.y += h;
        displaced.y -= 0.1;

        vec2 noise_uv = world_pos.xz * 0.5 + vec2(1.7, 3.2) * time * 0.3;
        displaced.y += fbm(noise_uv) * 0.05;

        vec3 tangent = normalize(vec3(eps, hx - h, 0.0));
        vec3 bitangent = normalize(vec3(0.0, hz - h, eps));
        vec3 normal = normalize(cross(bitangent, tangent));

        out_normal = normalize((model * vec4(normal, 0.0)).xyz);
    }

    out_world_pos = displaced;
    gl_Position = proj * view * vec4(displaced, 1.0);
}