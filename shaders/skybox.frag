#version 330 core

in vec3 sampleDirection;

uniform samplerCube uEnvironmentMap;
uniform mat3 uSampleRotation;
uniform float uIntensity;

out vec4 FragColor;

void main() {
    FragColor = vec4(texture(uEnvironmentMap, uSampleRotation * sampleDirection).rgb * uIntensity, 1.0);
}
