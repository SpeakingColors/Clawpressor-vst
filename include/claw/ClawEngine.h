// Clawpressor engine: plain C++ (no JUCE). Port of the design mockup DSP (docs/mockup/clawpressor.html):
// up to 4 phase aligned bands (LR crossovers 24 or 48 dB/oct), per band input gain, compressor with soft knee,
// auto release, upward lift, limiter, antialiased saturation, mid/side width, output gain; pattern gain; two
// modulation lanes; dry/wet mix; master output gain, 64 sample lookahead master limiter with smooth knee clipper,
// post gain. Every setter is cheap and called once per block; smoothing happens per sample.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "ClawDsp.h"

namespace claw {

inline double lin(double db) { return std::pow(10.0, db / 20.0); }
inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct BandP
{
    bool on = true, solo = false, mute = false, autoRel = false, soft = true, lim = true;
    float in = 0.f, thr = -18.f, ratio = 3.f, att = 10.f, rel = 120.f, boost = 0.f;
    float lthr = -3.f, lrel = 60.f, out = 0.f, sat = 0.f, wid = 100.f, knee = 6.f, ufl = -60.f, uamt = 1.f;
};
struct LaneP { bool on = false; int tgt = 0; float amt = 50.f; int st[16] = {}; };

// lane target ids: 0 mix, 1 master output, 2 post gain, then 3 + band * 4 + k with k 0 input, 1 output, 2 saturation, 3 width
struct Params
{
    int nb = 4, slope = 48;
    float x[3] = { 120.f, 1000.f, 6000.f };
    float mix = 50.f, out = 0.f, post = 0.f, mrel = 80.f;
    bool mlim = true, autog = true, rsteps = false;
    std::array<BandP, 4> b;
    bool pon = false; int pspeed = 0; float pstr = 60.f; bool px2 = false; int pat[16] = {};
    std::array<LaneP, 2> lane;
    float glide = 12.f;
};

// Meters: the audio thread raises the maxima, the editor reads and clears them (exchange(0)).
struct Meters
{
    std::atomic<float> inL { 0 }, inR { 0 }, outL { 0 }, outR { 0 }, rmsAcc { 0 }, mgr { 0 };
    std::array<std::atomic<float>, 4> gr, lgr, lvl;
    std::atomic<int> step { 0 }, rmsN { 0 };
    Meters() { for (auto& a : gr) a = 0.f; for (auto& a : lgr) a = 0.f; for (auto& a : lvl) a = 0.f; }
};
inline void raise(std::atomic<float>& a, float v) { if (v > a.load(std::memory_order_relaxed)) a.store(v, std::memory_order_relaxed); }

class Engine
{
public:
    static constexpr int kScope = 8192, kLook = 64;
    Params p;
    Meters meters;
    // band scope: mono, before (a) and after (b) processing; scopeIdx is the next write position
    std::array<std::array<float, kScope>, 4> scopeA {}, scopeB {};
    std::atomic<int> scopeIdx { 0 };
    int activeBands() const { return p.nb; }

    void prepare(double sampleRate)
    {
        sr = sampleRate; reset();
    }
    void reset()
    {
        for (auto& c : chains) for (auto& q : c.s) q.z[0][0] = q.z[0][1] = q.z[1][0] = q.z[1][1] = 0.0;
        gu.fill(0.0); gr.fill(0.0); lg.fill(0.0); gIn.fill(1.0); gOut.fill(1.0);
        for (auto& a : ad) a = Adaa{};
        lv[0] = lv[1] = 0.0; mixS = 0.5; gOutM = 1.0; gPost = 1.0; mg = 0.0; patG = 1.0; di = 0;
        std::fill(dl.begin(), dl.end(), 0.f);
        freeBeat = 0.0; dirty = true;
    }
    void markDirty() { dirty = true; }
    int latency() const { return kLook; }

    // beat0: position in quarter notes of the first sample of the block; the host tempo gives the beat per sample.
    void process(float* L, float* R, int n, double beat0, double bpm)
    {
        if (dirty) retune();
        const int nb = clampi(p.nb, 1, 4);
        bool anySolo = false; for (int b = 0; b < 4; ++b) anySolo = anySolo || p.b[b].solo;
        struct BP { bool on, soft, autoRel, lon, aud; double thr, R, iR, kw, ufl, up, sat, wid, attC, rel, mk, lthr, lrelC, gIn, gOut; } P[4];
        for (int b = 0; b < 4; ++b)
        {
            const auto& q = p.b[b];
            double R = q.ratio; if (p.rsteps) R = std::max(1.0, std::round(R)); if (R >= 19.95) R = 1e9;
            const double mk = p.autog ? clampd(-q.thr * (1.0 - 1.0 / std::min(R, 100.0)) * 0.5, 0.0, 24.0) : 0.0;
            P[b] = { q.on, q.soft, q.autoRel, q.lim, b < nb && !q.mute && (!anySolo || q.solo), q.thr, R, R > 1e8 ? 0.0 : 1.0 / R, q.soft ? q.knee : 0.0, q.ufl, q.uamt, q.sat * 0.01, q.wid * 0.01,
                     std::exp(-1.0 / (q.att * 0.001 * sr)), q.rel, mk + q.boost, lin(q.lthr), std::exp(-1.0 / (q.lrel * 0.001 * sr)), lin(q.in), lin(q.out) };
        }
        const double mixT = p.mix * 0.01, gOutT = lin(p.out), gPostT = lin(p.post), mrelC = std::exp(-1.0 / (p.mrel * 0.001 * sr)), ceil = lin(-0.3);
        const bool pOn = p.pon && p.pstr > 0.f;
        const double pDepth = p.pstr * 0.01 * (p.px2 ? 2.0 : 1.0), stepBeats = p.pspeed == 0 ? 0.25 : p.pspeed == 1 ? 0.5 : 1.0, pcoef = 1.0 - std::exp(-1.0 / (0.003 * sr));
        const double beatPerSample = bpm / 60.0 / sr, lcoef = 1.0 - std::exp(-1.0 / (std::max(1.0f, p.glide) * 0.001 * sr)), sm = 0.0025;
        static const double PD[4] = { 0, 4, 9, 16 };
        float pkIL = 0, pkIR = 0, pkOL = 0, pkOR = 0, mgMax = 0, grMax[4] = {}, lgMax[4] = {}, lvlMax[4] = {};
        double rms = 0.0;
        int tgtKind[2], tgtBand[2];
        for (int li = 0; li < 2; ++li)
        {
            const int t = p.lane[li].tgt; tgtKind[li] = t < 3 ? t : 3 + ((t - 3) & 3); tgtBand[li] = t < 3 ? 0 : clampi((t - 3) >> 2, 0, 3);
        }
        int sidx = scopeIdx.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const double xl = L[i], xr = R[i];
            pkIL = std::max(pkIL, (float) std::fabs(xl)); pkIR = std::max(pkIR, (float) std::fabs(xr));
            double dryL = 0, dryR = 0, wetL = 0, wetR = 0;
            double lmMix = 0, lmOut = 1, lmPost = 1, lmIn[4] = { 1, 1, 1, 1 }, lmOutB[4] = { 1, 1, 1, 1 }, lmSat[4] = {}, lmWid[4] = {};
            const double beat = beat0 + i * beatPerSample;
            const int ix = ((int) std::floor(beat / stepBeats)) & 15;
            for (int li = 0; li < 2; ++li)
            {
                const auto& ln = p.lane[li];
                if (!ln.on) { lv[li] += (0.0 - lv[li]) * lcoef; continue; }
                lv[li] += (ln.st[ix] / 4.0 - lv[li]) * lcoef;
                const double mv = lv[li] * ln.amt * 0.01; const int k = tgtKind[li], bb = tgtBand[li];
                if (k == 0) lmMix += mv; else if (k == 1) lmOut *= lin(mv * 24.0); else if (k == 2) lmPost *= lin(mv * 24.0);
                else if (k == 3) lmIn[bb] *= lin(mv * 24.0); else if (k == 4) lmOutB[bb] *= lin(mv * 24.0); else if (k == 5) lmSat[bb] += mv; else lmWid[bb] += mv * 2.0;
            }
            split(xl, 0, nb); split(xr, 1, nb);
            for (int b = 0; b < 4; ++b)
            {
                const auto& q = P[b];
                const double gi = gIn[b] += (q.gIn * lmIn[b] - gIn[b]) * sm, go = gOut[b] += (q.gOut * lmOutB[b] - gOut[b]) * sm;
                double yl = bb_[0][b] * gi, yr = bb_[1][b] * gi;
                { scopeA[b][(size_t) ((sidx) & (kScope - 1))] = (float) ((yl + yr) * 0.5); }
                if (q.aud) { dryL += yl; dryR += yr; }
                const double lvv = std::max(std::fabs(yl), std::fabs(yr)), ld = 20.0 * std::log10(lvv + 1e-9);
                if (lvv > lvlMax[b]) lvlMax[b] = (float) lvv;
                double g_ = gr[b];
                if (q.on)
                {
                    const double ov = ld - q.thr; double tg;
                    if (q.kw > 0 && 2 * ov > -q.kw && 2 * ov < q.kw) { const double t2 = ov + q.kw / 2; tg = (1 - q.iR) * t2 * t2 / (2 * q.kw); }
                    else if (ov > 0) tg = ov * (1 - q.iR); else tg = 0;
                    if (tg > g_) g_ += (tg - g_) * (1 - q.attC);
                    else { const double rc = std::exp(-1.0 / ((q.autoRel ? clampd(50 + 30 * g_, 50, 900) : q.rel) * 0.001 * sr)); g_ += (tg - g_) * (1 - rc); }
                    gr[b] = g_;
                    double gu_ = gu[b], tu = 0;
                    if (q.up > 1.04 && ld < q.ufl) { tu = std::min(24.0, (q.ufl - ld) * (1 - 1 / q.up)); tu *= clampd((ld - (q.ufl - 45)) / 12, 0, 1); }
                    if (tu > gu_) gu_ += (tu - gu_) * (1 - std::exp(-1.0 / (std::max((double) q.rel, 30.0) * 0.001 * sr))); else gu_ += (tu - gu_) * (1 - q.attC);
                    gu[b] = gu_;
                    const double g = lin(q.mk - g_ + gu_); yl *= g; yr *= g;
                }
                else { g_ = 0; gr[b] = 0; gu[b] = 0; }
                if (g_ > grMax[b]) grMax[b] = (float) g_;
                double lg_ = lg[b];
                if (q.lon)
                {
                    const double pk = std::max(std::fabs(yl), std::fabs(yr)), lt = pk > q.lthr ? 20.0 * std::log10(pk / q.lthr) : 0.0;
                    lg_ = lt > lg_ ? lt : lg_ * q.lrelC; lg[b] = lg_; const double g2 = lin(-lg_); yl *= g2; yr *= g2;
                }
                else { lg_ = 0; lg[b] = 0; }
                if (lg_ > lgMax[b]) lgMax[b] = (float) lg_;
                const double sa = clampd(q.sat + lmSat[b], 0, 1);
                if (sa > 0.002)
                {
                    const double gd = lin(sa * 18.0);
                    yl = yl * (1 - sa) + ad[b].tick(0, yl * gd) / std::sqrt(gd) * sa;
                    yr = yr * (1 - sa) + ad[b].tick(1, yr * gd) / std::sqrt(gd) * sa;
                }
                else { ad[b].sync(0, yl); ad[b].sync(1, yr); }
                const double wd = clampd(q.wid + lmWid[b], 0, 2);
                if (std::fabs(wd - 1) > 0.002) { const double mm = 0.5 * (yl + yr), ss = 0.5 * (yl - yr) * wd; yl = mm + ss; yr = mm - ss; }
                yl *= go; yr *= go;
                scopeB[b][(size_t) (sidx & (kScope - 1))] = (float) ((yl + yr) * 0.5);
                if (q.aud) { wetL += yl; wetR += yr; }
            }
            ++sidx;
            double pg = 1.0;
            if (pOn) { pg = lin(-PD[std::min(3, std::max(0, p.pat[ix]))] * pDepth); step = ix; }
            patG += (pg - patG) * pcoef; wetL *= patG; wetR *= patG;
            const double mx = mixS += (clampd(mixT + lmMix, 0, 1) - mixS) * sm;
            double oL2 = dryL + (wetL - dryL) * mx, oR2 = dryR + (wetR - dryR) * mx;
            const double go2 = gOutM += (gOutT * lmOut - gOutM) * sm; oL2 *= go2; oR2 *= go2;
            // lookahead delay line (kLook frames), master limiter, smooth knee clipper
            double rl = dl[(size_t) di * 2], rr = dl[(size_t) di * 2 + 1]; dl[(size_t) di * 2] = (float) oL2; dl[(size_t) di * 2 + 1] = (float) oR2; di = (di + 1) % kLook;
            if (p.mlim)
            {
                const double pkm = std::max(std::fabs(oL2), std::fabs(oR2)), lt2 = pkm > ceil ? 20.0 * std::log10(pkm / ceil) : 0.0;
                if (lt2 > mg) mg += (lt2 - mg) * 0.25; else mg = lt2 + (mg - lt2) * mrelC;
                const double gm = lin(-mg); rl *= gm; rr *= gm;
                rl = softClip((float) rl, (float) ceil); rr = softClip((float) rr, (float) ceil);
            }
            else mg = 0;
            if (mg > mgMax) mgMax = (float) mg;
            const double gp = gPost += (gPostT * lmPost - gPost) * sm; rl *= gp; rr *= gp;
            L[i] = (float) rl; R[i] = (float) rr;
            pkOL = std::max(pkOL, (float) std::fabs(rl)); pkOR = std::max(pkOR, (float) std::fabs(rr)); rms += rl * rl + rr * rr;
        }
        scopeIdx.store(sidx & (kScope - 1), std::memory_order_release);
        raise(meters.inL, pkIL); raise(meters.inR, pkIR); raise(meters.outL, pkOL); raise(meters.outR, pkOR); raise(meters.mgr, mgMax);
        for (int b = 0; b < 4; ++b) { raise(meters.gr[b], grMax[b]); raise(meters.lgr[b], lgMax[b]); raise(meters.lvl[b], lvlMax[b]); }
        meters.rmsAcc.store(meters.rmsAcc.load(std::memory_order_relaxed) + (float) (rms / 2.0), std::memory_order_relaxed);
        meters.rmsN.store(meters.rmsN.load(std::memory_order_relaxed) + n, std::memory_order_relaxed);
        meters.step.store(step, std::memory_order_relaxed);
    }

private:
    struct Biq { double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z[2][2] = {}; };
    struct Adaa
    {
        double x1[2] = { 0, 0 }, F1[2] = { 0, 0 };
        double tick(int c, double x) { const double F = lcosh(x), dx = x - x1[c]; const double y = std::fabs(dx) < 1e-5 ? std::tanh(0.5 * (x + x1[c])) : (F - F1[c]) / dx; x1[c] = x; F1[c] = F; return y; }
        void sync(int c, double x) { x1[c] = x; F1[c] = lcosh(x); }
    };
    static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static double lcosh(double x) { return claw::lcosh(x); }

    // 12 filter chains: lp1 hp1 lp2 hp2 a2lp a2hp lp3 hp3 a31lp a31hp a32lp a32hp, each up to 4 biquads
    enum { LP1, HP1, LP2, HP2, A2LP, A2HP, LP3, HP3, A31LP, A31HP, A32LP, A32HP, NF };
    struct Chain { Biq s[4]; int count = 2; };
    void coef(Biq& s, bool lp, double fc, double q)
    {
        const double w = 6.283185307179586 * fc / sr, c = std::cos(w), sn = std::sin(w), al = sn / (2 * q), a0 = 1 + al;
        double b0, b1, b2; if (lp) { b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0; } else { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; }
        s.b0 = b0 / a0; s.b1 = b1 / a0; s.b2 = b2 / a0; s.a1 = -2 * c / a0; s.a2 = (1 - al) / a0;
    }
    void tune(int idx, bool lp, double fc)
    {
        static const double q48[4] = { 0.5412, 1.3066, 0.5412, 1.3066 }, q24[2] = { 0.7071, 0.7071 };
        auto& ch = chains[idx]; ch.count = p.slope == 48 ? 4 : 2;
        for (int i = 0; i < ch.count; ++i) coef(ch.s[i], lp, fc, p.slope == 48 ? q48[i] : q24[i]);
    }
    void retune()
    {
        const double f1 = clampd(p.x[0], 30, 16000), f2 = clampd(p.x[1], 30, 16000), f3 = clampd(p.x[2], 30, 16000);
        tune(LP1, true, f1); tune(HP1, false, f1); tune(LP2, true, f2); tune(HP2, false, f2); tune(A2LP, true, f2); tune(A2HP, false, f2);
        tune(LP3, true, f3); tune(HP3, false, f3); tune(A31LP, true, f3); tune(A31HP, false, f3); tune(A32LP, true, f3); tune(A32HP, false, f3);
        dirty = false;
    }
    double run(int idx, double x, int c) { auto& ch = chains[idx]; for (int i = 0; i < ch.count; ++i) { auto& s = ch.s[i]; const double y = s.b0 * x + s.z[c][0]; s.z[c][0] = s.b1 * x - s.a1 * y + s.z[c][1]; s.z[c][1] = s.b2 * x - s.a2 * y; x = y; } return x; }
    void split(double x, int c, int nb)
    {
        double* B = bb_[c];
        if (nb == 1) { B[0] = x; return; }
        const double l1 = run(LP1, x, c), h1 = run(HP1, x, c);
        if (nb == 2) { B[0] = l1; B[1] = h1; return; }
        const double l2 = run(LP2, h1, c), h2 = run(HP2, h1, c), a1 = run(A2LP, l1, c) + run(A2HP, l1, c);
        if (nb == 3) { B[0] = a1; B[1] = l2; B[2] = h2; return; }
        const double l3 = run(LP3, h2, c), h3 = run(HP3, h2, c);
        B[0] = run(A31LP, a1, c) + run(A31HP, a1, c); B[1] = run(A32LP, l2, c) + run(A32HP, l2, c); B[2] = l3; B[3] = h3;
    }

    double sr = 44100.0;
    std::array<Chain, NF> chains;
    double bb_[2][4] = {};
    std::array<double, 4> gu {}, gr {}, lg {}, gIn { 1, 1, 1, 1 }, gOut { 1, 1, 1, 1 };
    std::array<Adaa, 4> ad;
    double lv[2] = { 0, 0 }, mixS = 0.5, gOutM = 1.0, gPost = 1.0, mg = 0.0, patG = 1.0, freeBeat = 0.0;
    std::array<float, kLook * 2> dl {};
    int di = 0, step = 0;
    bool dirty = true;
};

} // namespace claw
