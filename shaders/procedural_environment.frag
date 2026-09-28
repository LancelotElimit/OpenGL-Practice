#version 330 core

in vec3 localDirection;
out vec4 FragColor;

void main() {
    vec3 direction = normalize(localDirection);
    float skyAmount = smoothstep(-0.15, 0.65, direction.y);
    vec3 ground = vec3(0.025, 0.018, 0.015);
    vec3 horizon = vec3(0.42, 0.20, 0.10);
    vec3 zenith = vec3(0.025, 0.12, 0.42);
    vec3 sky = mix(horizon, zenith, max(direction.y, 0.0));
    vec3 color = mix(ground, sky, skyAmount);

    vec3 sunDirection = normalize(vec3(-0.45, 0.72, 0.28));
    float sunCore = pow(max(dot(direction, sunDirection), 0.0), 1024.0);
    float sunGlow = pow(max(dot(direction, sunDirection), 0.0), 32.0);
    color += vec3(18.0, 12.0, 5.0) * sunCore;
    color += vec3(1.5, 0.55, 0.12) * sunGlow;

    FragColor = vec4(color, 1.0);
}
