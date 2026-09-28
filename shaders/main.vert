#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec4 aTangent;
layout (location = 4) in mat4 aInstanceModel;

uniform mat4 uViewProjection;
uniform mat4 uModel;
uniform bool uUseInstancing;
uniform mat4 uLightSpaceMatrix;

out vec2 texCoord;
out vec3 worldNormal;
out vec3 worldTangent;
out float tangentHandedness;
out vec3 worldPosition;
out vec4 lightSpacePosition;

void main() {
    mat4 modelMatrix = uUseInstancing ? aInstanceModel : uModel;
    vec4 worldVertex = modelMatrix * vec4(aPos, 1.0);
    gl_Position = uViewProjection * worldVertex;
    texCoord = aTexCoord;
    worldNormal = mat3(transpose(inverse(modelMatrix))) * aNormal;
    worldTangent = mat3(modelMatrix) * aTangent.xyz;
    tangentHandedness = aTangent.w;
    worldPosition = vec3(worldVertex);
    lightSpacePosition = uLightSpaceMatrix * worldVertex;
}
