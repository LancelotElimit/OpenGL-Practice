#version 330 core
in vec2 vUv;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uSceneDepth;
uniform bool uSoft;
float linearDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return 2.0 * 0.1 * 100.0 / (100.0 + 0.1 - z * (100.0 - 0.1));
}
void main() {
    // Analytic radial texture: no external particle image is required.
    float radius = length(vUv * 2.0 - 1.0);
    float alpha = 1.0 - smoothstep(0.15, 1.0, radius);
    if (uSoft) {
        vec2 screenUv = gl_FragCoord.xy / vec2(textureSize(uSceneDepth, 0));
        float sceneDepth = linearDepth(texture(uSceneDepth, screenUv).r);
        float particleDepth = linearDepth(gl_FragCoord.z);
        alpha *= clamp((sceneDepth - particleDepth) / 0.35, 0.0, 1.0);
    }
    FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
