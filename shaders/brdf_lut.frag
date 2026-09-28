#version 330 core

in vec2 texCoord;
out vec2 FragColor;

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

vec3 importanceSampleGgx(vec2 samplePoint, float roughness) {
    float alpha = roughness * roughness;
    float phi = 2.0 * PI * samplePoint.x;
    float cosTheta = sqrt(
        (1.0 - samplePoint.y)
        / (1.0 + (alpha * alpha - 1.0) * samplePoint.y)
    );
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float geometrySchlickGgx(float nDotV, float roughness) {
    float k = roughness * roughness * 0.5;
    return nDotV / (nDotV * (1.0 - k) + k);
}

float geometrySmith(float nDotV, float nDotL, float roughness) {
    return geometrySchlickGgx(nDotV, roughness)
        * geometrySchlickGgx(nDotL, roughness);
}

void main() {
    float nDotV = texCoord.x;
    float roughness = texCoord.y;
    vec3 viewDirection = vec3(
        sqrt(max(1.0 - nDotV * nDotV, 0.0)),
        0.0,
        nDotV
    );
    float scale = 0.0;
    float bias = 0.0;
    const uint sampleCount = 256u;

    for (uint index = 0u; index < sampleCount; ++index) {
        vec3 halfVector = importanceSampleGgx(
            hammersley(index, sampleCount),
            roughness
        );
        vec3 lightDirection = normalize(
            2.0 * dot(viewDirection, halfVector) * halfVector
            - viewDirection
        );
        float nDotL = max(lightDirection.z, 0.0);
        float nDotH = max(halfVector.z, 0.0);
        float vDotH = max(dot(viewDirection, halfVector), 0.0);
        if (nDotL <= 0.0) {
            continue;
        }

        float geometry = geometrySmith(
            nDotV,
            nDotL,
            roughness
        );
        float visibility = geometry * vDotH
            / max(nDotH * nDotV, 0.0001);
        float fresnel = pow(1.0 - vDotH, 5.0);
        scale += (1.0 - fresnel) * visibility;
        bias += fresnel * visibility;
    }

    FragColor = vec2(scale, bias) / float(sampleCount);
}
