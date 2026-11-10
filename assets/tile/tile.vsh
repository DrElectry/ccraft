#version 330 core

layout(location = 0) in uvec2 packed_face;

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

void unpack_face(uint face, out vec3 q0, out vec3 q1, out vec3 q2, out vec3 q3,
                 out vec3 N, out vec3 T, out vec3 B, out int reverse)
{
    reverse = 0;
    T = vec3(1.0, 0.0, 0.0);
    B = vec3(0.0, 1.0, 0.0);

    if (face == 0u) {
        q0 = vec3(0.0, 0.0, 1.0); q1 = vec3(1.0, 0.0, 1.0);
        q2 = vec3(1.0, 1.0, 1.0); q3 = vec3(0.0, 1.0, 1.0);
        N = vec3(0.0, 0.0, 1.0);
        T = vec3(1.0, 0.0, 0.0); B = vec3(0.0, 1.0, 0.0);
    } else if (face == 1u) {
        q0 = vec3(1.0, 0.0, 0.0); q1 = vec3(0.0, 0.0, 0.0);
        q2 = vec3(0.0, 1.0, 0.0); q3 = vec3(1.0, 1.0, 0.0);
        N = vec3(0.0, 0.0, -1.0);
        T = vec3(-1.0, 0.0, 0.0); B = vec3(0.0, 1.0, 0.0);
    } else if (face == 2u) {
        q0 = vec3(0.0, 0.0, 0.0); q1 = vec3(0.0, 0.0, 1.0);
        q2 = vec3(0.0, 1.0, 1.0); q3 = vec3(0.0, 1.0, 0.0);
        N = vec3(-1.0, 0.0, 0.0);
        T = vec3(0.0, 0.0, 1.0); B = vec3(0.0, 1.0, 0.0);
    } else if (face == 3u) {
        q0 = vec3(1.0, 0.0, 1.0); q1 = vec3(1.0, 0.0, 0.0);
        q2 = vec3(1.0, 1.0, 0.0); q3 = vec3(1.0, 1.0, 1.0);
        N = vec3(1.0, 0.0, 0.0);
        T = vec3(0.0, 0.0, -1.0); B = vec3(0.0, 1.0, 0.0);
    } else if (face == 4u) {
        q0 = vec3(0.0, 1.0, 1.0); q1 = vec3(1.0, 1.0, 1.0);
        q2 = vec3(1.0, 1.0, 0.0); q3 = vec3(0.0, 1.0, 0.0);
        N = vec3(0.0, 1.0, 0.0);
        T = vec3(1.0, 0.0, 0.0); B = vec3(0.0, 0.0, -1.0);
    } else if (face == 5u) {
        q0 = vec3(0.0, 0.0, 0.0); q1 = vec3(1.0, 0.0, 0.0);
        q2 = vec3(1.0, 0.0, 1.0); q3 = vec3(0.0, 0.0, 1.0);
        N = vec3(0.0, -1.0, 0.0);
        T = vec3(1.0, 0.0, 0.0); B = vec3(0.0, 0.0, 1.0);
    } else if (face == 6u || face == 7u) {
        q0 = vec3(0.0, 0.0, 0.0); q1 = vec3(1.0, 0.0, 1.0);
        q2 = vec3(1.0, 1.0, 1.0); q3 = vec3(0.0, 1.0, 0.0);
        N = (face == 6u) ? vec3(-0.707, 0.0, 0.707) : vec3(0.707, 0.0, -0.707);
        reverse = (face == 7u) ? 1 : 0;
    } else {
        q0 = vec3(1.0, 0.0, 0.0); q1 = vec3(0.0, 0.0, 1.0);
        q2 = vec3(0.0, 1.0, 1.0); q3 = vec3(1.0, 1.0, 0.0);
        N = (face == 8u) ? vec3(-0.707, 0.0, -0.707) : vec3(0.707, 0.0, 0.707);
        reverse = (face == 9u) ? 1 : 0;
    }
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

    vec3 q0, q1, q2, q3, N, T, B;
    int reverse;
    unpack_face(face, q0, q1, q2, q3, N, T, B, reverse);

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

    vec4 world_pos = model * vec4(in_vert, 1.0);
    vec4 view_pos = view * world_pos;
    out_view_pos = view_pos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 nW = normalize(normalMatrix * N);
    vec3 tW = normalize(normalMatrix * T);
    vec3 bW = normalize(normalMatrix * B);

    tW = normalize(tW - dot(tW, nW) * nW);
    bW = normalize(cross(nW, tW));

    out_normal = nW;
    out_tangent = tW;
    out_bitangent = bW;

    vec3 viewDirView = normalize(-view_pos.xyz);
    mat3 viewToWorld = transpose(mat3(view));
    vec3 viewDirWorld = viewToWorld * viewDirView;
    mat3 TBN = mat3(tW, bW, nW);
    TangentViewDir = transpose(TBN) * viewDirWorld;

    gl_Position = proj * view_pos;
}
