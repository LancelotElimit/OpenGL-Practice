#version 330 core
in vec2 vUv;
in vec4 vColor;
out vec4 FragColor;
void main() {
    if (vColor.a <= 0.0) discard;
    float r = length(vUv * 2.0 - 1.0);
    float opacity = 1.0 - smoothstep(0.1, 1.0, r);
    FragColor = vec4(vColor.rgb, vColor.a * opacity);
}
