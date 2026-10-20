// Clawpressor factory presets. Values are parameter ids mapped to plain values (choices and toggles as 0/1/index).
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace claw
{
using Values = std::vector<std::pair<juce::String, float>>;
struct Preset { juce::String name; Values v; };

// lane target ids: 0 mix, 1 master output, 2 post gain, 3 + band * 4 + k (k: 0 input, 1 output, 2 saturation, 3 width)
inline const juce::StringArray& laneTargetNames()
{
    static const juce::StringArray n = []
    {
        juce::StringArray a { "Mix", "Master output", "Post gain" };
        for (int b = 1; b <= 4; ++b) { a.add("Band " + juce::String(b) + " input"); a.add("Band " + juce::String(b) + " output");
                                       a.add("Band " + juce::String(b) + " saturation"); a.add("Band " + juce::String(b) + " width"); }
        return a;
    }();
    return n;
}

struct BandSpec
{
    bool on_ = true, au_ = false, lon_ = true; float t_ = -18, r_ = 3, a_ = 10, rl_ = 120, bo_ = 0, lt_ = -3, kn_ = 6, uf_ = -60, ua_ = 1, sa_ = 0, wi_ = 100;
    BandSpec& on(bool v) { on_ = v; return *this; } BandSpec& au(bool v) { au_ = v; return *this; } BandSpec& lon(bool v) { lon_ = v; return *this; }
    BandSpec& t(float v) { t_ = v; return *this; }   BandSpec& r(float v) { r_ = v; return *this; }   BandSpec& a(float v) { a_ = v; return *this; }
    BandSpec& rl(float v) { rl_ = v > 0 ? v : 120.f; return *this; } BandSpec& bo(float v) { bo_ = v; return *this; } BandSpec& lt(float v) { lt_ = v; return *this; }
    BandSpec& kn(float v) { kn_ = v; return *this; } BandSpec& uf(float v) { uf_ = v; return *this; } BandSpec& ua(float v) { ua_ = v; return *this; }
    BandSpec& sa(float v) { sa_ = v; return *this; } BandSpec& wi(float v) { wi_ = v; return *this; }
};
inline BandSpec B() { return {}; }
inline BandSpec NEUT() { return BandSpec().t(0).r(1).lon(false).lt(0); }

struct GSpec
{
    float mix = 50, x1 = 120, x2 = 1000, x3 = 6000, pstr = 60, la_amt = 50, lb_amt = 60, pglide = 12; int nb = 4, slope = 48, la_tgt = 0, lb_tgt = 9; bool mlim = true, pon = false;
    GSpec& Mix(float v) { mix = v; return *this; } GSpec& X(float a, float b, float c) { x1 = a; x2 = b; x3 = c; return *this; } GSpec& X1(float a) { x1 = a; return *this; }
    GSpec& Nb(int v) { nb = v; return *this; } GSpec& Mlim(bool v) { mlim = v; return *this; } GSpec& Pon(float s) { pon = true; pstr = s; return *this; }
    GSpec& LaA(int tgt, float amt) { la_tgt = tgt; la_amt = amt; return *this; } GSpec& Glide(float v) { pglide = v; return *this; }
};

inline Preset makePreset(const juce::String& name, const GSpec& g, std::vector<BandSpec> bands, const std::vector<int>& pat = {},
                         const std::vector<int>& laneA = {}, const std::vector<int>& laneB = {})
{
    Preset p; p.name = name; auto& v = p.v;
    auto add = [&](const juce::String& id, float x) { v.emplace_back(id, x); };
    add("nb", (float) (g.nb - 1)); add("slope", g.slope == 48 ? 1.f : 0.f); add("x1", g.x1); add("x2", g.x2); add("x3", g.x3);
    add("mix", g.mix); add("out", 0.f); add("post", 0.f); add("mlim", g.mlim ? 1.f : 0.f); add("mrel", 80.f); add("autog", 1.f); add("rsteps", 0.f);
    add("pon", g.pon ? 1.f : 0.f); add("pstr", g.pstr); add("px2", 0.f); add("pspeed", 0.f); add("pglide", g.pglide);
    add("la_tgt", (float) g.la_tgt); add("lb_tgt", (float) g.lb_tgt); add("la_amt", g.la_amt); add("lb_amt", g.lb_amt);
    add("la_on", laneA.empty() ? 0.f : 1.f); add("lb_on", laneB.empty() ? 0.f : 1.f);
    bands.resize(4, BandSpec());
    for (int b = 0; b < 4; ++b)
    {
        const auto& q = bands[(size_t) b]; const juce::String s = "b" + juce::String(b + 1) + "_";
        add(s + "on", q.on_ ? 1.f : 0.f); add(s + "solo", 0.f); add(s + "mute", 0.f); add(s + "in", 0.f); add(s + "thr", q.t_); add(s + "ratio", q.r_);
        add(s + "att", q.a_); add(s + "rel", q.rl_); add(s + "auto", q.au_ ? 1.f : 0.f); add(s + "soft", 1.f); add(s + "boost", q.bo_);
        add(s + "lon", q.lon_ ? 1.f : 0.f); add(s + "lthr", q.lt_); add(s + "lrel", 60.f); add(s + "out", 0.f); add(s + "knee", q.kn_);
        add(s + "ufl", q.uf_); add(s + "uamt", q.ua_); add(s + "sat", q.sa_ * 100.f); add(s + "wid", q.wi_);
    }
    for (int i = 0; i < 16; ++i)
    {
        add("pat" + juce::String(i), pat.empty() ? 0.f : (float) pat[(size_t) i]);
        add("la_s" + juce::String(i), laneA.empty() ? 0.f : (float) laneA[(size_t) i]);
        add("lb_s" + juce::String(i), laneB.empty() ? 0.f : (float) laneB[(size_t) i]);
    }
    return p;
}

inline std::vector<Preset> factoryPresets()
{
    using V = std::vector<BandSpec>;
    return {
        makePreset("Init (does nothing)", GSpec().Mix(100).Mlim(false), V { NEUT(), NEUT(), NEUT(), NEUT() }),
        makePreset("Neutral Glue", GSpec().Mix(100), V { B().t(-20).r(1.8f).a(30).rl(200), B().t(-20).r(2).a(20).rl(150), B().t(-20).r(2).a(15).rl(120), B().t(-22).r(1.8f).a(8).rl(100) }),
        makePreset("Dance Glue (low band off)", GSpec().Mix(70).X(110, 900, 5500), V { B().on(false).t(0).r(1).lon(false), B().t(-22).r(3).a(8).au(true).bo(1), B().t(-22).r(3).a(5).au(true).bo(1), B().t(-24).r(2.5f).a(3).rl(80).bo(1) }),
        makePreset("Upward Starter (OTT style)", GSpec().Mix(45).X(130, 2500, 7000), V { B().t(-30).r(4).a(5).rl(90).bo(2), B().t(-32).r(5).a(3).rl(70).bo(3), B().t(-32).r(5).a(2).rl(60).bo(3), B().t(-30).r(4).a(1).rl(50).bo(2) }),
        makePreset("Hardstyle Lock", GSpec().Mix(85).X(100, 700, 5000), V { B().t(-16).r(4).a(20).rl(110).lt(-2), B().t(-20).r(4).a(10).rl(80).bo(1), B().t(-20).r(5).a(4).rl(60).bo(2), B().t(-22).r(3).a(2).rl(50).bo(1) }),
        makePreset("UK Hardcore Punch", GSpec().Mix(75).X(140, 1200, 6500), V { B().t(-14).r(3).a(25).rl(130), B().t(-18).r(4).a(12).rl(90), B().t(-20).r(4).a(6).rl(70).bo(1), B().t(-22).r(3).a(3).rl(50) }),
        makePreset("Kick Beef (low only)", GSpec().Mix(100).Nb(2).X1(160), V { B().t(-18).r(5).a(30).rl(120).bo(1.5f).lt(-1), NEUT() }),
        makePreset("Pump 4x4 (pattern)", GSpec().Mix(100).Pon(70), V { B().t(-16).r(2).a(15).rl(120), B().t(-18).r(2).a(10).rl(100), B().t(-18).r(2).a(8).rl(90), B().t(-20).r(2).a(5).rl(80) }, { 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0 }),
        makePreset("Tame the Highs", GSpec().Mix(100), V { NEUT(), NEUT(), B().t(-26).r(3).a(3).rl(60), B().t(-28).r(5).a(1).rl(40).lt(-6) }),
        makePreset("Trance Lead Dense", GSpec().Mix(70).X(140, 1400, 6500), V { B().on(false).t(0).r(1).lon(false), B().t(-24).r(3).a(8).au(true).bo(1).kn(10), B().t(-24).r(3.5f).a(4).rl(90).bo(1.5f).ua(1.6f).uf(-42), B().t(-26).r(3).a(2).rl(60).sa(0.18f).wi(130) }),
        makePreset("Bass: mono lows + saturation", GSpec().Mix(100).Nb(3).X(110, 900, 6000), V { B().t(-18).r(4).a(25).rl(110).sa(0.3f).wi(0).lt(-2), B().t(-20).r(3).a(10).rl(80).sa(0.2f).bo(1).wi(90), B().t(-24).r(2.5f).a(4).rl(60).wi(120) }),
        makePreset("Lane Demo: saturation pulse", GSpec().Mix(100), V { B().t(-20).r(2.5f), B().t(-22).r(2.5f).sa(0.05f), B().t(-22).r(2.5f), B().t(-24).r(2.5f) }, {}, {}, { 4, 0, 0, 2, 4, 0, 0, 2, 4, 0, 0, 2, 4, 0, 2, 0 }),
        makePreset("Lane Demo: mix offbeat swell", GSpec().Mix(40).LaA(0, 60).Glide(40), V { B().t(-22).r(3), B().t(-22).r(3), B().t(-22).r(3), B().t(-22).r(3) }, {}, { 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0 }),
    };
}
}
