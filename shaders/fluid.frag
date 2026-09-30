#version 330 core
in vec3 vWorld;
in vec3 vNormal;
uniform vec3 uCamera;
uniform vec3 uLight;
uniform vec3 uLightColor;
uniform mat3 uEnvironmentRotation;
uniform float uEnvironmentIntensity;
uniform vec2 uViewport;
uniform float uOpacity;
uniform float uRefraction;
uniform float uAbsorption;
uniform float uReflectivity;
uniform float uFoam;
uniform float uRoughness;
uniform int uShadingMode;
uniform bool uPoints;
uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform samplerCube uEnvironment;
out vec4 FragColor;

float linearDepth(float z) {
    float ndc = z * 2.0 - 1.0;
    return (2.0 * 0.1 * 100.0) / (100.0 + 0.1 - ndc * (100.0 - 0.1));
}

void main() {
    if (uPoints) {
        vec2 center = gl_PointCoord * 2.0 - 1.0;
        if (dot(center, center) > 1.0) discard;
        FragColor = vec4(0.15, 0.75, 1.0, 0.9);
        return;
    }
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamera - vWorld);
    vec3 L = normalize(uLight - vWorld);
    vec2 uv = gl_FragCoord.xy / uViewport;
    vec2 displacedUv = clamp(uv + N.xz * uRefraction,
                             vec2(0.001), vec2(0.999));
    // Do not pull foreground geometry over the water silhouette.
    if (texture(uSceneDepth, displacedUv).r < gl_FragCoord.z - 0.0005)
        displacedUv = uv;

    vec3 background = texture(uSceneColor, displacedUv).rgb;
    float behind = texture(uSceneDepth, displacedUv).r;
    float thickness = clamp(linearDepth(behind) - linearDepth(gl_FragCoord.z),
                            0.0, 3.0);
    float transmittance = exp(-uAbsorption * thickness);
    vec3 refracted = mix(vec3(0.015, 0.18, 0.29), background, transmittance);

    float fresnel = 0.02 + 0.98 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 reflected = textureLod(uEnvironment, uEnvironmentRotation * reflect(-V, N),
                                uRoughness * 4.0).rgb * uEnvironmentIntensity;
    vec3 color = mix(refracted, reflected,
                     clamp(fresnel * uReflectivity, 0.0, 1.0));
    vec3 H = normalize(V + L);
    color += vec3(1.2, 1.25, 1.35) * uLightColor / 3.0
        * pow(max(dot(N, H), 0.0), mix(150.0, 16.0, uRoughness))
        * (1.0 - uRoughness);
    float shallow = 1.0 - smoothstep(0.015, 0.24, thickness);
    color += vec3(0.44, 0.61, 0.66) * shallow * uFoam;

    if (uShadingMode == 1) color = refracted;
    else if (uShadingMode == 2) color = vec3(fresnel);
    else if (uShadingMode == 3) color = N * 0.5 + 0.5;
    // Background was already mixed above; blending it again would double it.
    FragColor = vec4(mix(background, color, uOpacity), 1.0);
}
