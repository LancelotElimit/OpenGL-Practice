#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aVelocity;
layout (location = 2) in float aAge;
layout (location = 3) in float aLifetime;
layout (location = 4) in float aSeed;
uniform float uDt;
uniform float uTime;
uniform float uLifetime;
uniform float uGravity;
uniform float uRate;
uniform int uCount;
uniform int uPreset;
uniform bool uEmitting;
out vec3 outPosition;
out vec3 outVelocity;
out float outAge;
out float outLifetime;
out float outSeed;
float hash(float x) { return fract(sin(x * 127.1) * 43758.5453); }
void main() {
    outPosition = aPosition;
    outVelocity = aVelocity;
    outAge = aAge + uDt;
    outLifetime = uLifetime;
    outSeed = aSeed;
    if (outAge >= outLifetime) {
        float cycle = max(outLifetime, float(uCount) / max(uRate, 1.0));
        outAge = -(cycle - outLifetime);
    }
    if (aAge <= 0.0 && outAge >= 0.0 && uEmitting) {
        float r0 = hash(aSeed + floor(uTime * 13.0));
        float r1 = hash(aSeed * 3.17 + floor(uTime * 7.0));
        float r2 = hash(aSeed * 8.43 + floor(uTime * 11.0));
        outAge = 0.0;
        outPosition = vec3(-1.45, -0.2, 0.25);
        if (uPreset == 1) {
            outPosition += vec3((r0 - 0.5) * 2.2, 2.1, (r1 - 0.5) * 1.5);
            outVelocity = vec3((r2 - 0.5) * 0.35, -0.8 - r1, (r0 - 0.5) * 0.2);
        } else if (uPreset == 2) {
            outVelocity = vec3((r0 - 0.5) * 1.5, 2.5 + r1 * 2.2, (r2 - 0.5) * 1.5);
        } else {
            outVelocity = vec3((r0 - 0.5) * 3.0, 1.6 + r1 * 3.0, (r2 - 0.5) * 3.0);
        }
    } else if (outAge >= 0.0 && outAge < outLifetime) {
        outVelocity.y += uGravity * uDt;
        outPosition += outVelocity * uDt;
    }
    if (!uEmitting && aAge < 0.0) outAge = -0.01;
    gl_Position = vec4(0.0);
}
