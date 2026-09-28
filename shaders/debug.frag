#version 330 core

in vec2 texCoord;
uniform sampler2D uDepthMap;
out vec4 FragColor;

void main() {
    float depth = texture(uDepthMap, texCoord).r;
    FragColor = vec4(vec3(depth), 1.0);
}
