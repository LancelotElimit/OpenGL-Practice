#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 localDirection;

void main() {
    localDirection = aPos;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
