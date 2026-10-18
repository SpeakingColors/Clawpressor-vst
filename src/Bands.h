#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// Per band compressor: soft knee, peak detector with attack and release, makeup boost.
struct BandComp
{
    float thresholdDb = -20.f, ratio = 4.f, kneeDb = 6.f, attackMs = 5.f, releaseMs = 80.f, boostDb = 0.f;
    bool on = true;
    float grDb = 0.f; // last gain reduction in dB (positive number), for meters

    void prepare(double sr) { fs = sr; env = 0.f; }

    // static curve: input level in dB -> output level in dB (same shape as the mockup)
    float curve(float inDb) const
    {
        const float d = inDb - thresholdDb;
        if (2.f * d < -kneeDb) return inDb;
        if (2.f * std::abs(d) <= kneeDb) { const float x = d + kneeDb * .5f; return inDb + (1.f / ratio - 1.f) * x * x / (2.f * kneeDb); }
        return thresholdDb + d / ratio;
    }

    void process(juce::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples(), ch = b.getNumChannels();
        const float aA = std::exp(-1.f / (0.001f * attackMs * (float) fs + 1e-6f));
        const float aR = std::exp(-1.f / (0.001f * releaseMs * (float) fs + 1e-6f));
        const float boost = juce::Decibels::decibelsToGain(boostDb);
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.f;
            for (int c = 0; c < ch; ++c) peak = std::max(peak, std::abs(b.getReadPointer(c)[i]));
            const float inDb = juce::Decibels::gainToDecibels(peak, -100.f);
            const float want = on ? std::max(0.f, inDb - curve(inDb)) : 0.f;
            const float a = want > env ? aA : aR;
            env = a * env + (1.f - a) * want;
            const float g = juce::Decibels::decibelsToGain(-env) * (on ? boost : 1.f);
            for (int c = 0; c < ch; ++c) b.getWritePointer(c)[i] *= g;
        }
        grDb = env;
    }
    float fs = 44100.f, env = 0.f;
};
