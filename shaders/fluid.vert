#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
uniform mat4 uViewProjection;
uniform bool uPoints;
out vec3 vWorld;
out vec3 vNormal;
void main() {
    vWorld = aPosition;
    vNormal = aNormal;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
    if (uPoints) gl_PointSize = 9.0;
}
