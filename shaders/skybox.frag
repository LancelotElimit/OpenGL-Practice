#version 330 core

in vec3 sampleDirection;

uniform samplerCube uEnvironmentMap;

out vec4 FragColor;

void main() {
    FragColor = vec4(texture(uEnvironmentMap, sampleDirection).rgb, 1.0);
}
