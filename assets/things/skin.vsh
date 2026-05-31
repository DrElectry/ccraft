#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aBoneIDs;
layout(location = 4) in vec4 aWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout(std140) uniform BoneData {
    mat4 bones[64];
};

out vec2 out_uv;
out vec3 out_normal;
out vec3 out_view_pos;

void main() { // thats where we deform our model
    ivec4 ids = ivec4(aBoneIDs);
    float weight_sum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    vec4 skinned_pos;
    mat4 skinMatrix = mat4(1.0);
    if (weight_sum >= 0.0001) {
        vec4 w = aWeights / weight_sum;
        skinMatrix =
            w.x * bones[ids.x] +
            w.y * bones[ids.y] +
            w.z * bones[ids.z] +
            w.w * bones[ids.w];
        skinned_pos = skinMatrix * vec4(aPos, 1.0);
    } else {
        skinned_pos = vec4(aPos, 1.0);
    }

    vec4 world_pos = model * skinned_pos;
    vec4 view_pos = view * world_pos;

    gl_Position = projection * view_pos;

    out_uv = aUV;

    mat3 normalMatrix = mat3(transpose(inverse(mat3(model * skinMatrix))));
    out_normal = normalMatrix * aNormal;

    out_view_pos = view_pos.xyz;
}

