#pragma once

struct FluidSettings {
    bool visible = false, showWindow = false, paused = false, pour = false;
    float pourRate = 25, gravity = 9.8f, pressure = 12, viscosity = .12f;
    float opacity = .78f, refraction = .025f, absorption = 1.1f;
    float reflectivity = .75f, foam = .45f, roughness = .12f;
    int surfaceHz = 30, shadingMode = 0, viewMode = 0;
};
struct Fluid2DSettings {
    bool showWindow = true, enabled = true, paused = false, autoSource = true, centerObstacle = true;
    float sourceStrength = 1, force = .8f, dissipation = .995f, brushRadius = .045f;
    int pressureIterations = 24, viewMode = 0;
    float opacity = .7f;
};
