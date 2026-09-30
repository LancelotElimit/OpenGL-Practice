#pragma once

// Data-only components: scene objects do not depend on OpenGL resources.
struct ParticleSettings {
    bool enabled = true;
    float rate = 45.0f;
    float lifetime = 2.4f;
    float speed = 2.2f;
    float gravity = -2.5f;
    float startSize = 0.12f;
    float endSize = 0.02f;
    int preset = 0; // sparks, smoke, snow
    bool soft = true;
};

struct GpuParticleSettings {
    bool visible = true;
    bool emitting = true;
    bool paused = false;
    int capacity = 8192;
    float emissionRate = 5500.0f;
    float lifetime = 2.5f;
    float gravity = -1.5f;
    float size = 0.055f;
    int preset = 0; // sparks, snow, fountain
};
