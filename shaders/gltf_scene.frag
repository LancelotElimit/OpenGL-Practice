#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUv;
out vec4 FragColor;
uniform vec3 uCamera;
uniform vec3 uLight;
uniform vec3 uLightColor;
uniform float uAmbientIntensity;
uniform vec4 uBaseFactor;
uniform vec3 uEmissiveFactor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAlphaCutoff;
uniform int uAlphaMode;
uniform bool uHasBase;
uniform bool uHasMetalRough;
uniform bool uHasNormal;
uniform bool uHasOcclusion;
uniform bool uHasEmissive;
uniform sampler2D uBase;
uniform sampler2D uMetalRough;
uniform sampler2D uNormal;
uniform sampler2D uOcclusion;
uniform sampler2D uEmissive;
const float PI = 3.14159265359;
void main() {
    vec4 base = uBaseFactor;
    if (uHasBase) {
        vec4 sampleColor = texture(uBase, vUv);
        base.rgb *= pow(sampleColor.rgb, vec3(2.2)); // sRGB -> linear
        base.a *= sampleColor.a;
    }
    if (uAlphaMode == 1 && base.a < uAlphaCutoff) discard;
    if (uAlphaMode == 0) base.a = 1.0;
    float metallic = uMetallic;
    float roughness = uRoughness;
    if (uHasMetalRough) {
        vec4 mr = texture(uMetalRough, vUv);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = clamp(roughness, 0.045, 1.0);
    vec3 N = normalize(gl_FrontFacing ? vNormal : -vNormal);
    if (uHasNormal) {
        vec3 dp1 = dFdx(vWorld), dp2 = dFdy(vWorld);
        vec2 duv1 = dFdx(vUv), duv2 = dFdy(vUv);
        vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
        vec3 B = normalize(-dp1 * duv2.x + dp2 * duv1.x);
        vec3 mapN = texture(uNormal, vUv).xyz * 2.0 - 1.0;
        N = normalize(mat3(T, B, N) * mapN);
    }
    vec3 V = normalize(uCamera - vWorld);
    vec3 delta = uLight - vWorld;
    float distance2 = max(dot(delta, delta), 1.0);
    vec3 L = normalize(delta);
    vec3 H = normalize(V + L);
    float NoL = max(dot(N,L), 0.0), NoV = max(dot(N,V), 0.001);
    float NoH = max(dot(N,H), 0.0), VoH = max(dot(V,H), 0.0);
    float a = roughness * roughness, a2 = a * a;
    float denom = NoH * NoH * (a2 - 1.0) + 1.0;
    float D = a2 / max(PI * denom * denom, 0.0001);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = NoL / (NoL * (1.0-k) + k) * NoV / (NoV * (1.0-k) + k);
    vec3 F0 = mix(vec3(0.04), base.rgb, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * base.rgb / PI;
    vec3 specular = D * G * F / max(4.0 * NoL * NoV, 0.001);
    float ao = uHasOcclusion ? texture(uOcclusion, vUv).r : 1.0;
    vec3 emissive = uEmissiveFactor;
    if (uHasEmissive) emissive *= pow(texture(uEmissive, vUv).rgb, vec3(2.2));
    vec3 color = (diffuse + specular) * NoL * (35.0 / 3.0) * uLightColor / distance2
        + base.rgb * 0.1 * ao * uAmbientIntensity + emissive;
    FragColor = vec4(color, base.a);
}
