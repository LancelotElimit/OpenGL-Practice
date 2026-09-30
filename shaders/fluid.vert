#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
uniform mat4 uViewProjection;
uniform mat4 uModel;
uniform bool uPoints;
out vec3 vWorld;
out vec3 vNormal;
void main() {
    vWorld = vec3(uModel * vec4(aPosition,1.0));
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uViewProjection * vec4(vWorld, 1.0);
    if (uPoints) gl_PointSize = 9.0;
}
