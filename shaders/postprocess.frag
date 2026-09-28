#version 330 core

in vec2 texCoord;

uniform sampler2D uSceneTexture;
uniform sampler2D uBloomTexture;
uniform bool uGrayscale;
uniform bool uBloomEnabled;
uniform float uExposure;

out vec4 FragColor;

void main() {
    vec3 hdrColor = texture(uSceneTexture, texCoord).rgb;
    if (uBloomEnabled) {
        hdrColor += texture(uBloomTexture, texCoord).rgb;
    }

    // Exponential tone mapping converts unbounded HDR values to 0..1.
    vec3 color = vec3(1.0) - exp(-hdrColor * uExposure);
    color = pow(color, vec3(1.0 / 2.2));
    if (uGrayscale) {
        float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = vec3(luminance);
    }
    FragColor = vec4(color, 1.0);
}
