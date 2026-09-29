#version 330 core
layout (location = 0) in vec2 aCorner;
layout (location = 1) in vec4 aPositionSize;
layout (location = 2) in vec4 aColor;
uniform mat4 uViewProjection;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
out vec2 vUv;
out vec4 vColor;
void main() {
    vec3 world = aPositionSize.xyz + aPositionSize.w
        * (aCorner.x * uCameraRight + aCorner.y * uCameraUp);
    gl_Position = uViewProjection * vec4(world, 1.0);
    vUv = aCorner * 0.5 + 0.5;
    vColor = aColor;
}
