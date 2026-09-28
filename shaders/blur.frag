#version 330 core

in vec2 texCoord;

uniform sampler2D uImage;
uniform bool uHorizontal;

out vec4 FragColor;

void main() {
    const float weight[5] = float[](
        0.227027,
        0.1945946,
        0.1216216,
        0.054054,
        0.016216
    );
    vec2 texelSize = 1.0 / vec2(textureSize(uImage, 0));
    vec3 result = texture(uImage, texCoord).rgb * weight[0];

    for (int index = 1; index < 5; ++index) {
        vec2 offset = uHorizontal
            ? vec2(texelSize.x * index, 0.0)
            : vec2(0.0, texelSize.y * index);
        result += texture(uImage, texCoord + offset).rgb * weight[index];
        result += texture(uImage, texCoord - offset).rgb * weight[index];
    }

    FragColor = vec4(result, 1.0);
}
