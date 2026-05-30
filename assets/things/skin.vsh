#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in ivec4 aBoneIDs;
layout(location = 4) in vec4 aWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 bones[100];

out vec2 uv;
out vec3 normal;
out vec3 fragPos;

void main() {
    mat4 boneTransform = bones[aBoneIDs.x] * aWeights.x;
    boneTransform += bones[aBoneIDs.y] * aWeights.y;
    boneTransform += bones[aBoneIDs.z] * aWeights.z;
    boneTransform += bones[aBoneIDs.w] * aWeights.w;
    
    vec4 localPos = boneTransform * vec4(aPos, 1.0);
    vec4 worldPos = model * localPos;
    vec4 viewPos = view * worldPos;
    
    gl_Position = projection * viewPos;
    
    uv = aUV;
    
    mat3 normalMatrix = mat3(transpose(inverse(model * boneTransform)));
    normal = normalize(normalMatrix * aNormal);
    fragPos = worldPos.xyz;
}