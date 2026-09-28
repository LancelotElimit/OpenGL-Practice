#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 4) in mat4 aInstanceModel;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform bool uUseInstancing;

void main() {
    mat4 modelMatrix = uUseInstancing ? aInstanceModel : uModel;
    gl_Position = uLightSpaceMatrix * modelMatrix * vec4(aPos, 1.0);
}
