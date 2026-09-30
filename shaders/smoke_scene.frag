#version 330 core
in vec2 vUv;
uniform sampler2D uDensity;
uniform float uOpacity;
out vec4 FragColor;
void main() {
    vec3 density=texture(uDensity,vUv).rgb;
    float alpha=clamp(max(density.r,max(density.g,density.b))*uOpacity,0,1);
    if(alpha<0.005) discard;
    FragColor=vec4(vec3(0.65,0.72,0.8),alpha);
}
