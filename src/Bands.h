#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>
#include <claw/ClawDsp.h>

// Per band compressor: soft knee, peak detector with attack and release, boost.
// Lookahead: the detector sees the signal now, the audio is delayed by lookSamples.
struct BandComp
{
    float thresholdDb = -20.f, ratio = 4.f, kneeDb = 6.f, attackMs = 5.f, releaseMs = 80.f, boostDb = 0.f;
    float liftFloorDb = -60.f, liftRatio = 1.f; // upward compression: lift signals under the floor (1 = off)
    float liftDb = 0.f;   // current lift in dB, for meters
    bool on = true;
    float grDb = 0.f;     // gain reduction in dB (positive), for meters
    float levelDb = -100.f; // band input level in dB, for meters

    static constexpr int kMaxLook = 2048;

    void prepare(double sr)
    {
        fs = (float) sr; env = 0.f; lift = 0.f; pos = 0;
        for (auto& r : ring) r.assign((size_t) kMaxLook, 0.f);
    }

    // static curve: input dB -> output dB (same shape as the mockup)
    float curve(float inDb) const
    {
        const float d = inDb - thresholdDb;
        if (2.f * d < -kneeDb) return inDb;
        if (2.f * std::abs(d) <= kneeDb) { const float x = d + kneeDb * .5f; return inDb + (1.f / ratio - 1.f) * x * x / (2.f * kneeDb); }
        return thresholdDb + d / ratio;
    }

    // in place; buf has at least 1 channel, up to 2
    void process(float* const* ch, int numCh, int n, int lookSamples)
    {
        lookSamples = juce::jlimit(0, kMaxLook - 1, lookSamples);
        const float aA = std::exp(-1.f / (0.001f * attackMs * fs + 1e-6f));
        const float aR = std::exp(-1.f / (0.001f * releaseMs * fs + 1e-6f));
        const float boost = juce::Decibels::decibelsToGain(boostDb);
        float lvl = 0.f;
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.f;
            for (int c = 0; c < numCh; ++c) peak = std::max(peak, std::abs(ch[c][i]));
            lvl = std::max(lvl, peak);
            const float inDb = juce::Decibels::gainToDecibels(peak, -100.f);
            const float want = on ? std::max(0.f, inDb - curve(inDb)) : 0.f;
            const float a = want > env ? aA : aR;
            env = a * env + (1.f - a) * want;
            float tu = on ? claw::upwardLiftDb(inDb, liftFloorDb, liftRatio) : 0.f;
            lift += (tu - lift) * (tu > lift ? (1.f - aR) : (1.f - aA));
            const float g = juce::Decibels::decibelsToGain(-env + lift) * (on ? boost : 1.f);
            for (int c = 0; c < numCh; ++c)
            {
                auto& r = ring[(size_t) c];
                r[(size_t) pos] = ch[c][i];
                const int rd = (pos - lookSamples + kMaxLook) % kMaxLook;
                ch[c][i] = r[(size_t) rd] * g;
            }
            pos = (pos + 1) % kMaxLook;
        }
        grDb = env; liftDb = lift;
        levelDb = juce::Decibels::gainToDecibels(lvl, -100.f);
    }

    float fs = 44100.f, env = 0.f, lift = 0.f;
    int pos = 0;
    std::array<std::vector<float>, 2> ring;
};

// Plain delay used to keep the dry path aligned with the lookahead.
struct DryDelay
{
    static constexpr int kMax = BandComp::kMaxLook;
    void prepare() { for (auto& r : ring) r.assign((size_t) kMax, 0.f); pos = 0; }
    void process(float* const* ch, int numCh, int n, int d)
    {
        d = juce::jlimit(0, kMax - 1, d);
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                auto& r = ring[(size_t) c];
                r[(size_t) pos] = ch[c][i];
                ch[c][i] = r[(size_t) ((pos - d + kMax) % kMax)];
            }
            pos = (pos + 1) % kMax;
        }
    }
    int pos = 0;
    std::array<std::vector<float>, 2> ring;
};
