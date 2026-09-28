#version 330 core

in vec3 localDirection;

uniform samplerCube uEnvironmentMap;

out vec4 FragColor;

const float PI = 3.14159265359;

void main() {
    vec3 normal = normalize(localDirection);
    vec3 upReference = abs(normal.y) < 0.999
        ? vec3(0.0, 1.0, 0.0)
        : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(upReference, normal));
    vec3 up = cross(normal, right);

    vec3 irradiance = vec3(0.0);
    float sampleCount = 0.0;
    const float sampleStep = 0.12;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleStep) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleStep) {
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            vec3 sampleDirection = tangentSample.x * right
                + tangentSample.y * up
                + tangentSample.z * normal;
            irradiance += texture(uEnvironmentMap, sampleDirection).rgb
                * cos(theta)
                * sin(theta);
            sampleCount += 1.0;
        }
    }

    irradiance = PI * irradiance / sampleCount;
    FragColor = vec4(irradiance, 1.0);
}
