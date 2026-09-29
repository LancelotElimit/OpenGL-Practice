#version 330 core
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D uVelocity;
uniform sampler2D uSource;
uniform sampler2D uPressure;
uniform sampler2D uDivergence;
uniform sampler2D uObstacle;
uniform int uMode;
uniform int uViewMode;
uniform float uDt;
uniform float uDissipation;
uniform float uRadius;
uniform vec2 uTexel;
uniform vec2 uSplatUv;
uniform vec4 uSplatValue;
float wall(vec2 uv) { return texture(uObstacle, uv).r; }
void main() {
    vec2 uv = texCoord;
    vec2 dx = vec2(uTexel.x, 0.0), dy = vec2(0.0, uTexel.y);
    if (uMode == 5) {
        if (uViewMode == 4) {
            float o = wall(uv);
            FragColor = vec4(vec3(o), 1.0);
        } else if (uViewMode == 1) {
            vec2 v = texture(uVelocity, uv).xy;
            FragColor = vec4(clamp(vec3(0.5 + v.x * 0.35,
                0.5 + v.y * 0.35, length(v) * 0.6), 0.0, 1.0), 1.0);
        } else if (uViewMode == 2) {
            float p = texture(uPressure, uv).r;
            FragColor = vec4(clamp(vec3(0.5 + p * 5.0, 0.5 - abs(p) * 2.0,
                0.5 - p * 5.0), 0.0, 1.0), 1.0);
        } else if (uViewMode == 3) {
            float d = texture(uDivergence, uv).r;
            FragColor = vec4(clamp(vec3(0.5 + d * 0.01, 0.5 - abs(d) * 0.005,
                0.5 - d * 0.01), 0.0, 1.0), 1.0);
        } else {
            vec3 density = texture(uSource, uv).rgb;
            vec3 background = vec3(0.025, 0.035, 0.07);
            FragColor = vec4(clamp(background + density * 1.4, 0.0, 1.0), 1.0);
        }
        return;
    }
    if (wall(uv) > 0.5) { FragColor = vec4(0.0); return; }
    if (uMode == 0) {
        vec2 velocity = texture(uVelocity, uv).xy;
        vec2 previous = clamp(uv - velocity * uDt,
            uTexel * 1.5, vec2(1.0) - uTexel * 1.5);
        if (wall(previous) > 0.5) previous = uv;
        FragColor = texture(uSource, previous) * uDissipation;
    } else if (uMode == 1) {
        float d = length((uv - uSplatUv) / max(uRadius, 0.001));
        float brush = exp(-d * d * 2.0);
        FragColor = texture(uSource, uv) + uSplatValue * brush;
        FragColor = clamp(FragColor, vec4(-2.0), vec4(3.0));
    } else if (uMode == 2) {
        vec2 left = wall(uv-dx) > 0.5 ? vec2(0.0) : texture(uVelocity, uv-dx).xy;
        vec2 right = wall(uv+dx) > 0.5 ? vec2(0.0) : texture(uVelocity, uv+dx).xy;
        vec2 bottom = wall(uv-dy) > 0.5 ? vec2(0.0) : texture(uVelocity, uv-dy).xy;
        vec2 top = wall(uv+dy) > 0.5 ? vec2(0.0) : texture(uVelocity, uv+dy).xy;
        float div = 0.5 * ((right.x-left.x)/uTexel.x + (top.y-bottom.y)/uTexel.y);
        FragColor = vec4(div, 0.0, 0.0, 1.0);
    } else if (uMode == 3) {
        float center = texture(uPressure, uv).r;
        float l = wall(uv-dx)>0.5 ? center : texture(uPressure, uv-dx).r;
        float r = wall(uv+dx)>0.5 ? center : texture(uPressure, uv+dx).r;
        float b = wall(uv-dy)>0.5 ? center : texture(uPressure, uv-dy).r;
        float t = wall(uv+dy)>0.5 ? center : texture(uPressure, uv+dy).r;
        float div = texture(uDivergence, uv).r;
        float h2 = uTexel.x * uTexel.x;
        FragColor = vec4((l+r+b+t-div*h2)*0.25, 0.0, 0.0, 1.0);
    } else {
        float center = texture(uPressure, uv).r;
        float l = wall(uv-dx)>0.5 ? center : texture(uPressure, uv-dx).r;
        float r = wall(uv+dx)>0.5 ? center : texture(uPressure, uv+dx).r;
        float b = wall(uv-dy)>0.5 ? center : texture(uPressure, uv-dy).r;
        float t = wall(uv+dy)>0.5 ? center : texture(uPressure, uv+dy).r;
        vec2 velocity = texture(uVelocity, uv).xy
            - vec2(r-l, t-b) / (2.0 * uTexel.x);
        if (wall(uv-dx)>0.5 || wall(uv+dx)>0.5) velocity.x = 0.0;
        if (wall(uv-dy)>0.5 || wall(uv+dy)>0.5) velocity.y = 0.0;
        FragColor = vec4(velocity, 0.0, 1.0);
    }
}
