#version 330 core
layout (location = 0) in vec2 aCorner;
layout (location = 1) in vec3 aPosition;
layout (location = 2) in vec3 aVelocity;
layout (location = 3) in float aAge;
layout (location = 4) in float aLifetime;
layout (location = 5) in float aSeed;
uniform mat4 uViewProjection;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform float uSize;
uniform int uPreset;
out vec2 vUv;
out vec4 vColor;
void main() {
    float t = clamp(aAge / max(aLifetime, 0.001), 0.0, 1.0);
    float size = uSize * mix(1.0, 0.25, t);
    vec3 world = aPosition + size * (aCorner.x * uCameraRight + aCorner.y * uCameraUp);
    gl_Position = uViewProjection * vec4(world, 1.0);
    vUv = aCorner * 0.5 + 0.5;
    if (uPreset == 1) vColor = vec4(0.55, 0.75, 1.0, 0.45 * (1.0-t));
    else if (uPreset == 2) vColor = vec4(0.15, 0.65, 1.0, 0.65 * (1.0-t));
    else vColor = vec4(2.0, 0.85 + 0.7 * (1.0-t), 0.08, 0.65 * (1.0-t));
    if (aAge < 0.0 || aAge >= aLifetime) vColor.a = 0.0;
}
