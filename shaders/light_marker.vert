#version 330 core

uniform mat4 uViewProjection;
uniform vec3 uLightPosition;

void main() {
    gl_Position = uViewProjection * vec4(uLightPosition, 1.0);
    gl_PointSize = 18.0;
}
