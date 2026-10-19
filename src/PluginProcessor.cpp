#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
const char* ClawpressorProcessor::lookNames[] = { "Off", "1 ms", "2.5 ms", "5 ms", "7.5 ms" };

namespace
{
struct Preset { const char* name; bool on[4]; float th[4], ratio[4], knee[4], att[4], rel[4], boost[4]; float mix; float ufl = -60.f, uamt = 1.f, sat[4] = {0, 0, 0, 0}, wid[4] = {100, 100, 100, 100}; };
const Preset kPresets[] = {
    { "Dance Master 2 style (low band off)", {false, true, true, true},
      {-20, -18, -20, -22}, {4, 4, 5, 6}, {6, 6, 4, 3}, {5, 3, 2, 1}, {80, 80, 60, 50}, {0, 0, 0, 0}, 50 },
    { "OTT style starter", {true, true, true, true},
      {-30, -30, -30, -30}, {8, 8, 8, 8}, {2, 2, 2, 2}, {1, 1, 1, 1}, {60, 60, 60, 60}, {2, 2, 2, 2}, 50 },
    { "Gentle glue", {true, true, true, true},
      {-14, -14, -14, -14}, {2, 2, 2, 2}, {10, 10, 10, 10}, {20, 20, 20, 20}, {200, 200, 200, 200}, {0, 0, 0, 0}, 100 },
    { "Mono bass, wide top", {true, true, true, true},
      {-22, -20, -20, -22}, {4, 3, 3, 4}, {6, 6, 6, 6}, {8, 5, 3, 2}, {120, 90, 70, 60}, {0, 0, 0, 0}, 80, -60.f, 1.f, {10, 0, 0, 0}, {0, 100, 140, 160} },
    { "Lift the tails", {true, true, true, true},
      {-16, -16, -16, -16}, {3, 3, 3, 3}, {8, 8, 8, 8}, {10, 8, 6, 4}, {150, 120, 100, 80}, {0, 0, 0, 0}, 70, -36.f, 2.5f, {0, 0, 0, 0}, {100, 100, 100, 100} },
    { "Warm and wide synth", {true, true, true, true},
      {-24, -22, -22, -24}, {3, 3, 3, 3}, {6, 6, 6, 6}, {6, 5, 4, 3}, {100, 90, 80, 70}, {1, 1, 1, 1}, 65, -60.f, 1.f, {20, 25, 20, 10}, {40, 110, 130, 150} },
};
constexpr int kNumPresets = (int) (sizeof(kPresets) / sizeof(kPresets[0]));
}

APVTS::ParameterLayout ClawpressorProcessor::layout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto f = [&](juce::String id, juce::String name, float lo, float hi, float def, float step = 0.f, float centre = 0.f)
    {
        juce::NormalisableRange<float> r(lo, hi, step);
        if (centre > 0.f) r.setSkewForCentre(centre);
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, r, def));
    };
    f("mix", "Dry/Wet", 0.f, 100.f, 50.f, 1.f);
    f("xo1", "Crossover 1", 40.f, 400.f, 120.f, 0.f, 120.f);
    f("xo2", "Crossover 2", 200.f, 3000.f, 800.f, 0.f, 800.f);
    f("xo3", "Crossover 3", 1500.f, 12000.f, 5000.f, 0.f, 5000.f);
    f("limRel", "Limiter release", 1.f, 1000.f, 150.f, 0.f, 150.f);
    f("post", "Post gain", -12.f, 12.f, 0.f, 0.1f);
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"clip", 1}, "Soft clip", true));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"look", 1}, "Lookahead", juce::StringArray(lookNames, 5), 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"speed", 1}, "Pattern speed", juce::StringArray{"1/16", "1/8", "1/4"}, 1));
    f("str", "Pattern strength", 0.f, 100.f, 0.f, 1.f);
    for (int s = 0; s < kSteps; ++s)
        p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pat" + juce::String(s), 1}, "Pattern step " + juce::String(s + 1), 0, 3, 0));
    const char* nm[] = {"Low", "LowMid", "HighMid", "High"};
    for (int b = 0; b < kBands; ++b)
    {
        const juce::String s = "b" + juce::String(b) + "_", n = juce::String(nm[b]) + " ";
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{s + "on", 1}, n + "On", b != 0));
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{s + "mute", 1}, n + "Mute", false));
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{s + "solo", 1}, n + "Solo", false));
        f(s + "th", n + "Threshold", -60.f, 0.f, -20.f, 0.5f);
        f(s + "ratio", n + "Ratio", 1.f, 20.f, 4.f, 0.5f);
        f(s + "knee", n + "Knee", 0.f, 24.f, 6.f, 0.5f);
        f(s + "att", n + "Attack", 0.1f, 100.f, 5.f, 0.f, 10.f);
        f(s + "rel", n + "Release", 5.f, 1000.f, 80.f, 0.f, 100.f);
        f(s + "boost", n + "Boost", 0.f, 12.f, 0.f, 0.5f);
        f(s + "ufl", n + "Lift floor", -60.f, -20.f, -60.f, 0.5f);
        f(s + "uamt", n + "Lift ratio", 1.f, 4.f, 1.f, 0.1f);
        f(s + "sat", n + "Saturation", 0.f, 100.f, 0.f, 1.f);
        f(s + "wid", n + "Width", 0.f, 200.f, 100.f, 1.f);
    }
    return { p.begin(), p.end() };
}

ClawpressorProcessor::ClawpressorProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", layout()) { applyPreset(0); }

bool ClawpressorProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

int ClawpressorProcessor::getNumPrograms() { return kNumPresets; }
const juce::String ClawpressorProcessor::getProgramName(int i) { return juce::isPositiveAndBelow(i, kNumPresets) ? kPresets[i].name : ""; }
void ClawpressorProcessor::setCurrentProgram(int i) { if (juce::isPositiveAndBelow(i, kNumPresets)) { program = i; applyPreset(i); } }

void ClawpressorProcessor::applyPreset(int i)
{
    const auto& pr = kPresets[i];
    auto set = [&](const juce::String& id, float v)
    { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
    for (int b = 0; b < kBands; ++b)
    {
        const juce::String s = "b" + juce::String(b) + "_";
        set(s + "on", pr.on[b] ? 1.f : 0.f); set(s + "th", pr.th[b]); set(s + "ratio", pr.ratio[b]); set(s + "knee", pr.knee[b]);
        set(s + "att", pr.att[b]); set(s + "rel", pr.rel[b]); set(s + "boost", pr.boost[b]);
    }
    for (int b = 0; b < kBands; ++b)
    {
        const juce::String s = "b" + juce::String(b) + "_";
        set(s + "ufl", pr.ufl); set(s + "uamt", pr.uamt); set(s + "sat", pr.sat[b]); set(s + "wid", pr.wid[b]);
    }
    set("mix", pr.mix);
}

void ClawpressorProcessor::prepareToPlay(double sr, int bs)
{
    fs = sr; maxBlock = bs;
    juce::dsp::ProcessSpec spec{sr, (juce::uint32) bs, 2};
    for (auto* f : { &s1, &s2, &s3, &a2a, &a3a, &a3b }) { f->prepare(spec); f->reset(); }
    for (auto& c : comp) c.prepare(sr);
    for (auto& b : bandData) for (auto& ch : b) ch.assign((size_t) bs, 0.f);
    for (auto& ch : dryData) ch.assign((size_t) bs, 0.f);
    dryDelay.prepare();
    limiter.prepare(spec);
    freePpq = 0.0; lastLatency = -1;
}

void ClawpressorProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    if (n > maxBlock) return; // host broke its promise; pass audio through untouched
    for (int c = 0; c < nch; ++c) std::copy(buf.getReadPointer(c), buf.getReadPointer(c) + n, dryData[c].begin());

    const float x1 = get("xo1"), x2 = juce::jmax(get("xo2"), x1 * 1.2f), x3 = juce::jmax(get("xo3"), x2 * 1.2f);
    s1.setCutoffFrequency(x1);
    s2.setCutoffFrequency(x2); a2a.setCutoffFrequency(x2);
    s3.setCutoffFrequency(x3); a3a.setCutoffFrequency(x3); a3b.setCutoffFrequency(x3);

    for (int c = 0; c < nch; ++c)
    {
        const float* in = buf.getReadPointer(c);
        for (int i = 0; i < n; ++i)
        {
            float lo1, hi1, l, h, mid1, hi2, b2, b3;
            s1.processSample(c, in[i], lo1, hi1);
            a2a.processSample(c, lo1, l, h); const float ap = l + h;
            a3a.processSample(c, ap, l, h);  bandData[0][c][(size_t) i] = l + h;
            s2.processSample(c, hi1, mid1, hi2);
            a3b.processSample(c, mid1, l, h); bandData[1][c][(size_t) i] = l + h;
            s3.processSample(c, hi2, b2, b3);
            bandData[2][c][(size_t) i] = b2; bandData[3][c][(size_t) i] = b3;
        }
    }

    const int lookIdx = juce::jlimit(0, 4, (int) get("look"));
    const int look = (int) std::lround(lookMs[lookIdx] * 0.001 * fs);
    if (look != lastLatency) { setLatencySamples(look); lastLatency = look; }

    bool anySolo = false;
    for (int b = 0; b < kBands; ++b) anySolo = anySolo || get("b" + juce::String(b) + "_solo") > .5f;

    float* wet[2] = { buf.getWritePointer(0), nch > 1 ? buf.getWritePointer(1) : nullptr };
    for (int c = 0; c < nch; ++c) std::fill(wet[c], wet[c] + n, 0.f);
    for (int b = 0; b < kBands; ++b)
    {
        const juce::String s = "b" + juce::String(b) + "_";
        auto& cp = comp[(size_t) b];
        cp.on = get(s + "on") > .5f; cp.thresholdDb = get(s + "th"); cp.ratio = get(s + "ratio");
        cp.liftFloorDb = get(s + "ufl"); cp.liftRatio = get(s + "uamt");
        cp.kneeDb = get(s + "knee"); cp.attackMs = get(s + "att"); cp.releaseMs = get(s + "rel"); cp.boostDb = get(s + "boost");
        float* ch[2] = { bandData[b][0].data(), bandData[b][1].data() };
        cp.process(ch, nch, n, look);
        {   // per band saturation (antialiased) and stereo width, after the compressor
            const float sat = get(s + "sat") * 0.01f, wd = get(s + "wid") * 0.01f;
            for (int i = 0; i < n; ++i)
            {
                ch[0][i] = claw::saturate(satL[(size_t) b], ch[0][i], sat);
                if (nch > 1) { ch[1][i] = claw::saturate(satR[(size_t) b], ch[1][i], sat); claw::width(ch[0][i], ch[1][i], wd); }
            }
        }
        grMeter[(size_t) b] = cp.grDb; lvMeter[(size_t) b] = cp.levelDb;
        const bool audible = get(s + "mute") < .5f && (!anySolo || get(s + "solo") > .5f);
        if (audible) for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) wet[c][i] += ch[c][i];
    }

    // pattern gain (applied to the wet signal, before the mix and the master limiter)
    const float strength = get("str") / 100.f;
    if (strength > 0.f)
    {
        const double beatsPerStep = get("speed") < .5f ? 0.25 : (get("speed") < 1.5f ? 0.5 : 1.0);
        double ppq = freePpq, bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto t = pos->getBpm()) bpm = *t;
                if (auto q = pos->getPpqPosition()) { ppq = *q; }
            }
        const double ppqPerSample = bpm / 60.0 / fs;
        const float coef = std::exp(-1.f / (0.002f * (float) fs));
        for (int i = 0; i < n; ++i)
        {
            const int step = (int) std::floor(ppq / beatsPerStep) % kSteps;
            const int lvl = (int) get("pat" + juce::String(step < 0 ? step + kSteps : step));
            const float target = juce::Decibels::decibelsToGain(-12.f * strength * (float) lvl / 3.f);
            patGain = coef * patGain + (1.f - coef) * target;
            for (int c = 0; c < nch; ++c) wet[c][i] *= patGain;
            ppq += ppqPerSample;
            if (i == 0) patStep = step < 0 ? step + kSteps : step;
        }
        freePpq = ppq;
    }
    else patGain = 1.f;

    const float mix = get("mix") / 100.f;
    float* dch[2] = { dryData[0].data(), dryData[1].data() };
    dryDelay.process(dch, nch, n, look);
    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < n; ++i) wet[c][i] = wet[c][i] * mix + dch[c][i] * (1.f - mix);

    limiter.setThreshold(-0.3f); limiter.setRelease(get("limRel"));
    juce::dsp::AudioBlock<float> blk(buf);
    limiter.process(juce::dsp::ProcessContextReplacing<float>(blk));
    buf.applyGain(juce::Decibels::decibelsToGain(get("post")));
    if (get("clip") > .5f)   // smooth knee clipper at the very end, ceiling 0.97 (just under full scale)
        for (int c = 0; c < nch; ++c) { auto* d = buf.getWritePointer(c); for (int i = 0; i < n; ++i) d[i] = claw::softClip(d[i], 0.97f); }
    float pk = buf.getMagnitude(0, n);
    masterGr = juce::jmax(0.f, juce::Decibels::gainToDecibels(pk, -100.f)); // output peak in dB, shown as the master meter
}

juce::AudioProcessorEditor* ClawpressorProcessor::createEditor() { return new ClawpressorEditor(*this); }
void ClawpressorProcessor::getStateInformation(juce::MemoryBlock& d)
{ if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d); }
void ClawpressorProcessor::setStateInformation(const void* d, int s)
{ if (auto xml = getXmlFromBinary(d, s)) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ClawpressorProcessor(); }
