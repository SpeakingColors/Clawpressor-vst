#include <claw/ClawEngine.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace claw;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s (line %d)\n", msg, __LINE__); ++fails; } } while (0)

static Params neutral(int nb, int slope)
{
    Params p; p.nb = nb; p.slope = slope; p.mix = 100.f; p.mlim = false; p.autog = false;
    for (auto& b : p.b) { b.on = false; b.lim = false; b.thr = 0.f; b.ratio = 1.f; }
    return p;
}
static void run(Engine& e, std::vector<float>& l, std::vector<float>& r, double bpm = 120.0, double beat0 = 0.0)
{
    const int blk = 256; double beat = beat0;
    for (size_t i = 0; i < l.size(); i += blk)
    {
        const int n = (int) std::min<size_t>(blk, l.size() - i);
        e.process(l.data() + i, r.data() + i, n, beat, bpm); beat += n * bpm / 60.0 / 48000.0;
    }
}
static double rmsRange(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double) (b - a)); }
static void sine(std::vector<float>& l, std::vector<float>& r, double f, double amp, double fs = 48000.0)
{ for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = (float) (amp * std::sin(6.283185307 * f * (double) i / fs)); }

int main()
{
    // 1. crossover sum is flat (all bands neutral, mix 100): output level equals input level for several tones, 24 and 48 dB/oct, 2..4 bands
    for (int slope : { 24, 48 }) for (int nb = 1; nb <= 4; ++nb) for (double f : { 60.0, 300.0, 1000.0, 4000.0, 12000.0 })
    {
        Engine e; e.prepare(48000.0); e.p = neutral(nb, slope); e.p.x[0] = 120.f; e.p.x[1] = 1000.f; e.p.x[2] = 6000.f; e.markDirty();
        std::vector<float> l(48000), r(48000); sine(l, r, f, 0.5); const double ri = rmsRange(l, 24000, 48000);
        run(e, l, r); const double ro = rmsRange(l, 24000, 48000);
        const double dbd = 20 * std::log10(ro / ri);
        if (std::fabs(dbd) > 0.15) { std::printf("flat: slope %d nb %d f %.0f diff %.3f dB\n", slope, nb, f, dbd); ++fails; }
    }
    // 2. compressor reduces level and makeup works
    {
        Engine e; e.prepare(48000.0); e.p = neutral(1, 24); e.p.b[0].on = true; e.p.b[0].thr = -30.f; e.p.b[0].ratio = 8.f; e.p.b[0].att = 1.f; e.p.b[0].rel = 50.f; e.p.b[0].knee = 0.f; e.p.b[0].soft = false;
        e.markDirty(); std::vector<float> l(48000), r(48000); sine(l, r, 1000.0, 0.5); const double ri = rmsRange(l, 24000, 48000);
        run(e, l, r); const double ro = rmsRange(l, 24000, 48000);
        CHECK(20 * std::log10(ro / ri) < -10.0, "compressor reduces a -6 dB tone with thr -30 ratio 8 by more than 10 dB");
        // autogain adds makeup
        Engine e2; e2.prepare(48000.0); e2.p = e.p; e2.p.autog = true; e2.markDirty(); std::vector<float> l2(48000), r2(48000); sine(l2, r2, 1000.0, 0.5); run(e2, l2, r2);
        CHECK(rmsRange(l2, 24000, 48000) < 0.0 || rmsRange(l2, 24000, 48000) > ro * 1.5, "autogain raises the level");
    }
    // 3. master limiter never exceeds the ceiling and reports latency 64
    {
        Engine e; e.prepare(48000.0); e.p = neutral(1, 24); e.p.mlim = true; e.p.out = 12.f; e.markDirty();
        std::vector<float> l(48000), r(48000); sine(l, r, 200.0, 0.9); run(e, l, r);
        float pk = 0; for (size_t i = 4000; i < l.size(); ++i) pk = std::max(pk, std::fabs(l[i]));
        CHECK(pk <= 0.9701f, "master limiter keeps output under -0.3 dB");
        CHECK(e.latency() == 64, "latency 64");
    }
    // 4. lane on mix target: offbeat steps move the mix
    {
        Engine e; e.prepare(48000.0); e.p = neutral(1, 24); e.p.mix = 0.f; e.p.lane[0].on = true; e.p.lane[0].tgt = 0; e.p.lane[0].amt = 100.f; e.p.glide = 1.f;
        for (int i = 0; i < 16; ++i) e.p.lane[0].st[i] = (i % 4 == 2) ? 4 : 0;
        e.p.b[0].on = true; e.p.b[0].thr = -60.f; e.p.b[0].ratio = 20.f; e.p.b[0].soft = false; e.markDirty();   // wet path very squashed so mix is audible
        std::vector<float> l(96000), r(96000); sine(l, r, 500.0, 0.5); run(e, l, r, 120.0);
        // 1/16 steps at 120 bpm are 0.125 s = 6000 samples; step 2 is 12000..18000 (mix up), step 0 is silent-wet = dry
        const double a = rmsRange(l, 3000, 5000), b = rmsRange(l, 15000, 17000);
        CHECK(std::fabs(a - b) > 0.05, "lane 2 on mix changes level between step 0 and step 2");
    }
    // 5. solo / mute / band off
    {
        Engine e; e.prepare(48000.0); e.p = neutral(4, 48); e.p.b[3].mute = true; e.markDirty();
        std::vector<float> l(48000), r(48000); sine(l, r, 12000.0, 0.5); run(e, l, r);
        CHECK(rmsRange(l, 24000, 48000) < 0.05, "muting the high band removes 12 kHz");
        Engine e2; e2.prepare(48000.0); e2.p = neutral(4, 48); e2.p.b[0].solo = true; e2.markDirty();
        std::vector<float> l2(48000), r2(48000); sine(l2, r2, 12000.0, 0.5); run(e2, l2, r2);
        CHECK(rmsRange(l2, 24000, 48000) < 0.05, "soloing the low band removes 12 kHz");
    }
    // 6. width 0 makes the band mono; saturation and no NaN under stress
    {
        Engine e; e.prepare(48000.0); e.p = neutral(1, 24); e.p.b[0].wid = 0.f; e.markDirty();
        std::vector<float> l(24000), r(24000); for (size_t i = 0; i < l.size(); ++i) { l[i] = (float) (0.5 * std::sin(0.05 * (double) i)); r[i] = -l[i]; }
        run(e, l, r); CHECK(rmsRange(l, 12000, 24000) < 1e-3, "width 0 sums opposite signals to silence");
        Engine s; s.prepare(44100.0); s.p = neutral(4, 48); for (auto& b : s.p.b) { b.on = true; b.lim = true; b.sat = 100.f; b.thr = -60.f; b.ratio = 20.f; b.ufl = -20.f; b.uamt = 4.f; b.wid = 200.f; }
        s.p.mlim = true; s.p.out = 12.f; s.markDirty();
        std::vector<float> l2(96000), r2(96000); srand(1); for (size_t i = 0; i < l2.size(); ++i) { l2[i] = (float) ((rand() / (double) RAND_MAX) * 2 - 1); r2[i] = (float) ((rand() / (double) RAND_MAX) * 2 - 1); }
        run(s, l2, r2); bool ok = true; for (size_t i = 0; i < l2.size(); ++i) if (!std::isfinite(l2[i]) || !std::isfinite(r2[i]) || std::fabs(l2[i]) > 1.0f) ok = false;
        CHECK(ok, "stress: finite and bounded");
    }
    // 7. pattern dips gain on marked steps
    {
        Engine e; e.prepare(48000.0); e.p = neutral(1, 24); e.p.pon = true; e.p.pstr = 100.f; e.p.pspeed = 0; for (int i = 0; i < 16; ++i) e.p.pat[i] = (i == 1) ? 3 : 0; e.markDirty();
        std::vector<float> l(96000), r(96000); sine(l, r, 500.0, 0.5); run(e, l, r, 120.0);
        CHECK(rmsRange(l, 9000, 11000) < rmsRange(l, 2000, 4000) * 0.5, "pattern step 2 dips the gain");
    }
    if (fails) { std::printf("%d failures\n", fails); return 1; }
    std::printf("engine tests ok\n"); return 0;
}
