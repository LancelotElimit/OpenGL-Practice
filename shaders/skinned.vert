#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in uvec4 aJoints;
layout (location = 2) in vec4 aWeights;

uniform mat4 uViewProjection;
uniform mat4 uModel;
uniform mat4 uBones[64];

out vec3 worldPosition;
out vec3 worldNormal;

void main() {
    mat4 skin = aWeights.x * uBones[aJoints.x]
        + aWeights.y * uBones[aJoints.y]
        + aWeights.z * uBones[aJoints.z]
        + aWeights.w * uBones[aJoints.w];
    vec4 worldVertex = uModel * skin * vec4(aPos, 1.0);
    worldPosition = worldVertex.xyz;
    worldNormal = normalize(
        mat3(transpose(inverse(uModel * skin))) * vec3(0.0, 0.0, 1.0)
    );
    gl_Position = uViewProjection * worldVertex;
}
