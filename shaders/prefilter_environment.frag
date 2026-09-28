#version 330 core

in vec3 localDirection;

uniform samplerCube uEnvironmentMap;
uniform float uRoughness;

out vec4 FragColor;

const float PI = 3.14159265359;

float radicalInverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u)
        | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u)
        | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u)
        | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u)
        | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint index, uint count) {
    return vec2(float(index) / float(count), radicalInverse(index));
}

vec3 importanceSampleGgx(vec2 samplePoint, vec3 normal, float roughness) {
    float alpha = roughness * roughness;
    float phi = 2.0 * PI * samplePoint.x;
    float cosTheta = sqrt(
        (1.0 - samplePoint.y)
        / (1.0 + (alpha * alpha - 1.0) * samplePoint.y)
    );
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    vec3 halfVector = vec3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );

    vec3 up = abs(normal.z) < 0.999
        ? vec3(0.0, 0.0, 1.0)
        : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    return normalize(
        tangent * halfVector.x
        + bitangent * halfVector.y
        + normal * halfVector.z
    );
}

float distributionGgx(vec3 normal, vec3 halfVector, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float nDotH = max(dot(normal, halfVector), 0.0);
    float denominator = nDotH * nDotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(PI * denominator * denominator, 0.000001);
}

void main() {
    vec3 normal = normalize(localDirection);
    vec3 viewDirection = normal;
    vec3 prefiltered = vec3(0.0);
    float totalWeight = 0.0;
    const uint sampleCount = 128u;

    for (uint index = 0u; index < sampleCount; ++index) {
        vec2 samplePoint = hammersley(index, sampleCount);
        vec3 halfVector = importanceSampleGgx(
            samplePoint,
            normal,
            uRoughness
        );
        vec3 lightDirection = normalize(
            2.0 * dot(viewDirection, halfVector) * halfVector
            - viewDirection
        );
        float nDotL = max(dot(normal, lightDirection), 0.0);
        if (nDotL <= 0.0) {
            continue;
        }

        float distribution = distributionGgx(
            normal,
            halfVector,
            uRoughness
        );
        float nDotH = max(dot(normal, halfVector), 0.0);
        float hDotV = max(dot(halfVector, viewDirection), 0.0);
        float pdf = distribution * nDotH / max(4.0 * hDotV, 0.0001);
        float sampleSolidAngle = 1.0 / (float(sampleCount) * pdf + 0.0001);
        float texelSolidAngle = 4.0 * PI / (6.0 * 256.0 * 256.0);
        float mipLevel = uRoughness == 0.0
            ? 0.0
            : 0.5 * log2(sampleSolidAngle / texelSolidAngle);

        prefiltered += textureLod(
            uEnvironmentMap,
            lightDirection,
            mipLevel
        ).rgb * nDotL;
        totalWeight += nDotL;
    }

    prefiltered /= max(totalWeight, 0.0001);
    FragColor = vec4(prefiltered, 1.0);
}
