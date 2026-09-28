#version 330 core

in vec3 localDirection;

uniform sampler2D uEquirectangularMap;

out vec4 FragColor;

const vec2 inverseAtan = vec2(0.15915494309, 0.31830988618);

void main() {
    vec3 direction = normalize(localDirection);
    vec2 uv = vec2(
        atan(direction.z, direction.x),
        asin(clamp(direction.y, -1.0, 1.0))
    );
    uv *= inverseAtan;
    uv += 0.5;
    FragColor = vec4(texture(uEquirectangularMap, uv).rgb, 1.0);
}
