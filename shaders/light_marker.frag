#version 330 core

uniform vec3 uMarkerColor;
out vec4 FragColor;

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    if (dot(centered, centered) > 1.0) {
        discard;
    }

    FragColor = vec4(uMarkerColor, 1.0);
}
