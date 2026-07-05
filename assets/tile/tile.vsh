#version 330 core

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_light;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

out vec2 out_uv;
out vec3 out_normal;
out vec3 out_tangent;
out vec3 out_bitangent;
out vec3 out_view_pos;
out float out_light;
out vec3 TangentViewDir;

void main()
{
    out_uv = in_uv;
    out_light = in_light;

    vec4 world_pos = model * vec4(in_vert, 1.0);
    vec4 view_pos = view * world_pos;
    out_view_pos = view_pos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * in_normal);
    vec3 T = normalize(normalMatrix * in_tangent);
    vec3 B = normalize(normalMatrix * in_bitangent);

    T = normalize(T - dot(T, N) * N);
    B = normalize(cross(N, T));

    out_normal = N;
    out_tangent = T;
    out_bitangent = B;

    vec3 viewDirView = normalize(-view_pos.xyz);
    mat3 viewToWorld = transpose(mat3(view));
    vec3 viewDirWorld = viewToWorld * viewDirView;
    mat3 TBN = mat3(T, B, N);
    TangentViewDir = transpose(TBN) * viewDirWorld;

    gl_Position = proj * view_pos;
}