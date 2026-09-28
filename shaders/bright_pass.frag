#version 330 core

in vec2 texCoord;

uniform sampler2D uSceneTexture;
uniform float uThreshold;

out vec4 FragColor;

void main() {
    vec3 color = texture(uSceneTexture, texCoord).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    FragColor = luminance > uThreshold
        ? vec4(color, 1.0)
        : vec4(0.0, 0.0, 0.0, 1.0);
}
