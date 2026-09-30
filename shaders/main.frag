#version 330 core

in vec2 texCoord;
in vec3 worldNormal;
in vec3 worldTangent;
in float tangentHandedness;
in vec3 worldPosition;
in vec4 lightSpacePosition;

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;
uniform sampler2D uShadowMap;
uniform samplerCube uPointShadowMap;
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D uBrdfLut;
uniform sampler2D uRoughnessMap;
uniform sampler2D uAoMap;
uniform vec3 uMaterialColor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAo;
uniform bool uUsePbrMaps;
uniform float uTextureScale;
uniform vec3 uLightPosition;
uniform vec3 uLightPosition2;
uniform vec3 uLightColor;
uniform vec3 uLightColor2;
uniform int uPointCount;
uniform vec3 uPointPositions[8];
uniform vec3 uPointColors[8];
uniform int uSpotCount;
uniform vec3 uSpotPositions[4];
uniform vec3 uSpotDirections[4];
uniform vec3 uSpotColors[4];
uniform float uEnvironmentIntensity;
uniform mat3 uEnvironmentRotation;
uniform bool uOverrideRoughness;
uniform vec3 uSpotLightPosition;
uniform vec3 uSpotLightDirection;
uniform vec3 uSpotLightColor;
uniform float uSpotInnerCutoff;
uniform float uSpotOuterCutoff;
uniform float uPointShadowFarPlane;
uniform vec3 uCameraPosition;

out vec4 FragColor;

const float PI = 3.14159265359;

float calculateSpotShadow(vec3 normal) {
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;
    if (projected.z > 1.0
        || projected.x < 0.0 || projected.x > 1.0
        || projected.y < 0.0 || projected.y > 1.0) {
        return 0.0;
    }
    float bias = max(
        0.002 * (1.0 - dot(normal, normalize(-uSpotLightDirection))),
        0.0005
    );
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(
                uShadowMap,
                projected.xy + vec2(x, y) * texelSize
            ).r;
            shadow += projected.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

float calculatePointShadow() {
    vec3 fragmentToLight = worldPosition - uLightPosition;
    float currentDepth = length(fragmentToLight);
    float sampledDepth = texture(uPointShadowMap, fragmentToLight).r;
    float nearPlane = 0.1;
    float depthNdc = sampledDepth * 2.0 - 1.0;
    float closestDepth = (2.0 * nearPlane * uPointShadowFarPlane)
        / (uPointShadowFarPlane + nearPlane
            - depthNdc * (uPointShadowFarPlane - nearPlane));
    return currentDepth - 0.05 > closestDepth ? 1.0 : 0.0;
}

vec3 calculateNormalFromMap() {
    vec3 normal = normalize(worldNormal);
    vec3 tangent = normalize(
        worldTangent - normal * dot(normal, worldTangent)
    );
    vec3 bitangent = normalize(cross(normal, tangent))
        * tangentHandedness;
    vec3 tangentNormal = texture(
        uNormalMap,
        texCoord * uTextureScale
    ).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;
    return normalize(mat3(tangent, bitangent, normal) * tangentNormal);
}

float distributionGgx(vec3 normal, vec3 halfVector, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float nDotH = max(dot(normal, halfVector), 0.0);
    float denominator = nDotH * nDotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(PI * denominator * denominator, 0.000001);
}

float geometrySchlickGgx(float nDotDirection, float roughness) {
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return nDotDirection / (nDotDirection * (1.0 - k) + k);
}

float geometrySmith(
    vec3 normal,
    vec3 viewDirection,
    vec3 lightDirection,
    float roughness
) {
    return geometrySchlickGgx(
        max(dot(normal, viewDirection), 0.0), roughness
    ) * geometrySchlickGgx(
        max(dot(normal, lightDirection), 0.0), roughness
    );
}

vec3 fresnelSchlick(float cosTheta, vec3 baseReflectance) {
    return baseReflectance
        + (1.0 - baseReflectance) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(
    float cosTheta,
    vec3 baseReflectance,
    float roughness
) {
    return baseReflectance
        + (max(vec3(1.0 - roughness), baseReflectance)
            - baseReflectance)
        * pow(1.0 - cosTheta, 5.0);
}

vec3 evaluateDirectLight(
    vec3 lightPosition,
    vec3 lightColor,
    vec3 normal,
    vec3 viewDirection,
    vec3 albedo,
    vec3 baseReflectance,
    float metallic,
    float roughness,
    float visibility
) {
    vec3 toLight = lightPosition - worldPosition;
    float distanceToLight = length(toLight);
    vec3 lightDirection = toLight / max(distanceToLight, 0.0001);
    vec3 halfVector = normalize(viewDirection + lightDirection);
    vec3 radiance = lightColor
        / max(distanceToLight * distanceToLight, 0.01);
    float distribution = distributionGgx(
        normal, halfVector, roughness
    );
    float geometry = geometrySmith(
        normal, viewDirection, lightDirection, roughness
    );
    vec3 fresnel = fresnelSchlick(
        max(dot(halfVector, viewDirection), 0.0), baseReflectance
    );
    vec3 specular = distribution * geometry * fresnel
        / max(
            4.0 * max(dot(normal, viewDirection), 0.0)
                * max(dot(normal, lightDirection), 0.0),
            0.0001
        );
    vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - metallic);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    return visibility
        * (diffuseWeight * albedo / PI + specular)
        * radiance * nDotL;
}

void main() {
    vec3 normal = calculateNormalFromMap();
    vec3 viewDirection = normalize(uCameraPosition - worldPosition);
    vec2 materialUv = texCoord * uTextureScale;
    vec3 sampledAlbedo = texture(uTexture, materialUv).rgb;
    vec3 albedo = pow(sampledAlbedo, vec3(2.2)) * uMaterialColor;
    float metallic = uMetallic;
    float roughness = uUsePbrMaps && !uOverrideRoughness
        ? texture(uRoughnessMap, materialUv).r
        : uRoughness;
    float ao = uUsePbrMaps
        ? texture(uAoMap, materialUv).r
        : uAo;
    roughness = clamp(roughness, 0.05, 1.0);
    vec3 baseReflectance = mix(vec3(0.04), albedo, metallic);

    vec3 directLighting = vec3(0);
    for(int i=0;i<uPointCount;++i) directLighting+=evaluateDirectLight(
        uPointPositions[i],uPointColors[i],normal,viewDirection,albedo,baseReflectance,metallic,roughness,
        i==0 ? 1.0-calculatePointShadow() : 1.0);

    for(int i=0;i<uSpotCount;++i) {
    vec3 spotDirection = normalize(uSpotPositions[i] - worldPosition);
    float theta = dot(spotDirection, normalize(-uSpotDirections[i]));
    float cone = clamp(
        (theta - uSpotOuterCutoff)
            / (uSpotInnerCutoff - uSpotOuterCutoff),
        0.0,
        1.0
    );
    directLighting += evaluateDirectLight(
        uSpotPositions[i], uSpotColors[i], normal, viewDirection,
        albedo, baseReflectance, metallic, roughness,
        cone * (i==0 ? 1.0 - calculateSpotShadow(normal) : 1.0)
    );
    }

    float nDotV = max(dot(normal, viewDirection), 0.0);
    vec3 fresnel = fresnelSchlickRoughness(
        nDotV, baseReflectance, roughness
    );
    vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - metallic);
    vec3 diffuseIbl = texture(uIrradianceMap, uEnvironmentRotation * normal).rgb * albedo;
    vec3 reflection = reflect(-viewDirection, normal);
    vec3 prefiltered = textureLod(
        uPrefilterMap, uEnvironmentRotation * reflection, roughness * 4.0
    ).rgb;
    vec2 brdf = texture(uBrdfLut, vec2(nDotV, roughness)).rg;
    vec3 specularIbl = prefiltered * (fresnel * brdf.x + brdf.y);
    vec3 ambient = (diffuseWeight * diffuseIbl + specularIbl) * ao * uEnvironmentIntensity;

    FragColor = vec4(ambient + directLighting, 1.0);
}
