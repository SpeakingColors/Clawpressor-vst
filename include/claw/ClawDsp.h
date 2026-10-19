// Pure C++ helpers for Clawpressor (no JUCE): ADAA saturation, upward lift, soft clipper, stereo width.
// Ported from the design mockup (docs/mockup/clawpressor.html) so the plugin and the mockup match.
#pragma once
#include <algorithm>
#include <cmath>

namespace claw {

inline double lcosh(double x) { x = std::fabs(x); return x + std::log1p(std::exp(-2.0 * x)) - 0.6931471805599453; }

// First order antiderivative antialiased tanh (Parker / Vicanek): y = (F(x) - F(x1)) / (x - x1), F = ln cosh.
struct Adaa {
    double x1 = 0.0, F1 = 0.0;
    double tick(double x) {
        const double F = lcosh(x), dx = x - x1;
        const double y = std::fabs(dx) < 1e-5 ? std::tanh(0.5 * (x + x1)) : (F - F1) / dx;
        x1 = x; F1 = F; return y;
    }
    void sync(double x) { x1 = x; F1 = lcosh(x); }
};

// amount 0..1: dry/wet of a drive of up to 18 dB, level compensated by 1/sqrt(gain)
inline float saturate(Adaa& a, float x, float amount) {
    if (amount < 0.002f) { a.sync(x); return x; }
    const double g = std::pow(10.0, amount * 18.0 / 20.0);
    const double y = a.tick(x * g) / std::sqrt(g);
    return float(x * (1.0 - amount) + y * amount);
}

// Upward compression lift in dB for a signal level in dB: quiet parts below the floor are raised,
// fading out 45 dB under the floor so silence and noise are not boosted. up = 1 is off.
inline float upwardLiftDb(float levelDb, float floorDb, float up) {
    if (up <= 1.04f || levelDb >= floorDb) return 0.f;
    float tu = std::min(24.f, (floorDb - levelDb) * (1.f - 1.f / up));
    const float fade = std::min(1.f, std::max(0.f, (levelDb - (floorDb - 45.f)) / 12.f));
    return tu * fade;
}

// Smooth knee clipper: identity below 72 % of the ceiling, C2 continuous above, never exceeds the ceiling.
inline float softClip(float x, float ceil) {
    const float a = std::fabs(x) / ceil, T = 0.72f;
    if (a <= T) return x;
    const float w = 1.f - T, y = T + w * std::tanh((a - T) / w);
    return (x < 0.f ? -y : y) * ceil;
}

// Mid/side width: 1 unchanged, 0 mono, 2 double side.
inline void width(float& l, float& r, float w) {
    if (std::fabs(w - 1.f) < 0.002f) return;
    const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
    l = m + s; r = m - s;
}

} // namespace claw
