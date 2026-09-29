#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUv;
uniform mat4 uViewProjection;
uniform mat4 uModel;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUv;
void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorld = world.xyz;
    vNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vUv = aUv;
    gl_Position = uViewProjection * world;
}
