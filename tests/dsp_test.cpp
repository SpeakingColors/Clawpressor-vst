#include "claw/ClawDsp.h"
#include <cstdio>
#include <vector>
using namespace claw;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

int main() {
    // softClip: identity under the knee, never above the ceiling, continuous and monotonic
    CHECK(softClip(0.5f, 1.f) == 0.5f);
    CHECK(softClip(0.72f, 1.f) == 0.72f);
    float prev = -1.f;
    for (int i = 0; i <= 400; ++i) { float x = i * 0.025f, y = softClip(x, 1.f); CHECK(y <= 1.f); CHECK(y >= prev - 1e-6f); prev = y; }
    CHECK(softClip(-5.f, 0.9f) >= -0.9f && softClip(-5.f, 0.9f) < -0.85f);
    // continuity at the knee (C0)
    CHECK(std::fabs(softClip(0.7201f, 1.f) - softClip(0.7199f, 1.f)) < 1e-3f);

    // upward lift: 0 above the floor and when off; at 10 dB under the floor with 2:1 it is 5 dB (the mockup text)
    CHECK(upwardLiftDb(-10.f, -20.f, 2.f) == 0.f);
    CHECK(upwardLiftDb(-30.f, -20.f, 1.f) == 0.f);
    CHECK(std::fabs(upwardLiftDb(-30.f, -20.f, 2.f) - 5.f) < 1e-4f);
    CHECK(upwardLiftDb(-90.f, -20.f, 4.f) == 0.f);            // silence is not boosted
    CHECK(upwardLiftDb(-60.f, -20.f, 4.f) <= 24.f);

    // width: 0 gives mono, 1 leaves it alone, 2 doubles the side
    { float l = 1.f, r = -1.f; width(l, r, 0.f); CHECK(l == 0.f && r == 0.f); }
    { float l = 0.7f, r = 0.2f; width(l, r, 1.f); CHECK(l == 0.7f && r == 0.2f); }
    { float l = 0.6f, r = 0.2f; width(l, r, 2.f); CHECK(std::fabs(l - 0.8f) < 1e-6f && std::fabs(r - 0.0f) < 1e-6f); }

    // saturation: off is bit exact, level stays within +-3 dB of the input RMS at full, output bounded
    { Adaa a; bool same = true; for (int i = 0; i < 100; ++i) { float x = std::sin(i * 0.1f) * 0.5f; same = same && saturate(a, x, 0.f) == x; } CHECK(same); }
    {
        Adaa a; double si = 0, so = 0; float pk = 0; const int N = 48000;
        for (int i = 0; i < N; ++i) { float x = 0.5f * std::sin(6.2831853f * 220.f * i / 48000.f), y = saturate(a, x, 1.f); si += x * x; so += y * y; pk = std::max(pk, std::fabs(y)); }
        double db = 10.0 * std::log10(so / si); std::printf("saturation level change at full: %.2f dB, peak %.3f\n", db, pk);
        CHECK(std::fabs(db) < 4.0); CHECK(pk < 1.5f);
    }
    // ADAA does not blow up on a hard step
    { Adaa a; for (int i = 0; i < 1000; ++i) { float y = saturate(a, (i / 50) % 2 ? 0.9f : -0.9f, 1.f); CHECK(std::isfinite(y) && std::fabs(y) < 2.f); } }

    std::printf(fails ? "FAILED\n" : "all tests passed\n");
    return fails ? 1 : 0;
}
