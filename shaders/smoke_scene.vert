#version 330 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aUv;
uniform mat4 uViewProjection;
uniform mat4 uModel;
out vec2 vUv;
void main() { vUv=aUv; gl_Position=uViewProjection*uModel*vec4(aPosition,0,1); }
