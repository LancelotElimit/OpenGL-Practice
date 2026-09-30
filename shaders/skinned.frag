#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;

uniform vec3 uCameraPosition;
uniform vec3 uLightPosition;
uniform vec3 uLightColor;
uniform float uAmbientIntensity;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(worldNormal);
    vec3 lightDirection = normalize(uLightPosition - worldPosition);
    vec3 viewDirection = normalize(uCameraPosition - worldPosition);
    vec3 halfVector = normalize(lightDirection + viewDirection);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfVector), 0.0), 48.0);
    vec3 baseColor = vec3(0.95, 0.28, 0.08);
    vec3 color = baseColor * 0.18 * uAmbientIntensity
        + (baseColor * 1.8 * diffuse + vec3(2.5, 1.6, 0.7) * specular) * uLightColor / 3.0;
    FragColor = vec4(color, 1.0);
}
