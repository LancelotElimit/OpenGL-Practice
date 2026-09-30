#pragma once
#include <algorithm>
#include <cmath>
class FrameClock {
public:
    explicit FrameClock(double initial=0):last_(initial) {}
    float tick(double now) {
        const auto elapsed=now-last_; last_=now;
        return std::isfinite(elapsed) && elapsed>0?static_cast<float>(elapsed):0.f;
    }
    static float simulationDelta(float elapsed) {
        return std::isfinite(elapsed)?std::clamp(elapsed,0.f,.05f):0.f;
    }
private:
    double last_;
};
