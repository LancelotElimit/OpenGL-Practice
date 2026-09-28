#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 sampleDirection;

void main() {
    sampleDirection = aPos;
    vec4 clipPosition = uProjection * mat4(mat3(uView)) * vec4(aPos, 1.0);
    gl_Position = clipPosition.xyww;
}
