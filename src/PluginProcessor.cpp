#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout ClawpressorProcessor::layout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto f = [&](juce::String id, juce::String name, float lo, float hi, float def, float step = 0.f)
    { p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(lo, hi, step), def)); };
    f("mix", "Dry/Wet", 0.f, 100.f, 50.f, 1.f);
    f("xo1", "Crossover 1", 40.f, 400.f, 120.f);
    f("xo2", "Crossover 2", 200.f, 3000.f, 800.f);
    f("xo3", "Crossover 3", 1500.f, 12000.f, 5000.f);
    f("limRel", "Limiter release", 1.f, 1000.f, 150.f);
    f("post", "Post gain", -12.f, 12.f, 0.f, 0.1f);
    const char* nm[] = {"Low", "LowMid", "HighMid", "High"};
    for (int b = 0; b < kBands; ++b)
    {
        const juce::String s = juce::String("b") + juce::String(b) + "_", n = juce::String(nm[b]) + " ";
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{s + "on", 1}, n + "On", b != 0)); // Dance Master 2 style: low band off
        f(s + "th", n + "Threshold", -60.f, 0.f, -20.f, 0.5f);
        f(s + "ratio", n + "Ratio", 1.f, 20.f, 4.f, 0.5f);
        f(s + "knee", n + "Knee", 0.f, 24.f, 6.f, 0.5f);
        f(s + "att", n + "Attack", 0.1f, 100.f, 5.f);
        f(s + "rel", n + "Release", 5.f, 1000.f, 80.f);
        f(s + "boost", n + "Boost", 0.f, 12.f, 0.f, 0.5f);
    }
    return { p.begin(), p.end() };
}

ClawpressorProcessor::ClawpressorProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", layout()) {}

void ClawpressorProcessor::prepareToPlay(double sr, int bs)
{
    juce::dsp::ProcessSpec spec{sr, (juce::uint32) bs, 2};
    for (auto& f : lowSplit)  { f.prepare(spec); f.setType(juce::dsp::LinkwitzRileyFilterType::lowpass); }
    for (auto& f : highSplit) { f.prepare(spec); f.setType(juce::dsp::LinkwitzRileyFilterType::highpass); }
    for (auto& c : comp) c.prepare(sr);
    for (auto& b : bandBuf) b.setSize(2, bs);
    rest.setSize(2, bs); dry.setSize(2, bs);
    limiter.prepare(spec);
}

void ClawpressorProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), ch = juce::jmin(2, buf.getNumChannels());
    dry.setSize(2, n, false, false, true); dry.clear();
    for (int c = 0; c < ch; ++c) dry.copyFrom(c, 0, buf, c, 0, n);
    const float x[3] = {*apvts.getRawParameterValue("xo1"), *apvts.getRawParameterValue("xo2"), *apvts.getRawParameterValue("xo3")};
    // split: band0 = LP(x1); rest = HP(x1); band1 = LP(x2) of rest; ...
    // Note: this simple cascade is not phase-aligned across bands; an all-pass compensation is a roadmap item.
    juce::AudioBuffer<float> work(2, n);
    work.makeCopyOf(dry, true);
    for (int i = 0; i < 3; ++i)
    {
        lowSplit[(size_t) i].setCutoffFrequency(x[i]);  highSplit[(size_t) i].setCutoffFrequency(x[i]);
        bandBuf[i].setSize(2, n, false, false, true); rest.setSize(2, n, false, false, true);
        bandBuf[i].makeCopyOf(work, true); rest.makeCopyOf(work, true);
        juce::dsp::AudioBlock<float> lo(bandBuf[i]), hi(rest);
        lowSplit[(size_t) i].process(juce::dsp::ProcessContextReplacing<float>(lo));
        highSplit[(size_t) i].process(juce::dsp::ProcessContextReplacing<float>(hi));
        work.makeCopyOf(rest, true);
    }
    bandBuf[3].setSize(2, n, false, false, true); bandBuf[3].makeCopyOf(work, true);

    buf.clear();
    for (int b = 0; b < kBands; ++b)
    {
        const juce::String s = juce::String("b") + juce::String(b) + "_";
        auto& c = comp[(size_t) b];
        c.on = *apvts.getRawParameterValue(s + "on") > .5f;
        c.thresholdDb = *apvts.getRawParameterValue(s + "th"); c.ratio = *apvts.getRawParameterValue(s + "ratio");
        c.kneeDb = *apvts.getRawParameterValue(s + "knee"); c.attackMs = *apvts.getRawParameterValue(s + "att");
        c.releaseMs = *apvts.getRawParameterValue(s + "rel"); c.boostDb = *apvts.getRawParameterValue(s + "boost");
        c.process(bandBuf[b]);
        grMeter[(size_t) b] = c.grDb;
        for (int k = 0; k < ch; ++k) buf.addFrom(k, 0, bandBuf[b], k, 0, n);
    }
    const float mix = *apvts.getRawParameterValue("mix") / 100.f;
    for (int k = 0; k < ch; ++k) { buf.applyGain(k, 0, n, mix); buf.addFrom(k, 0, dry, k, 0, n, 1.f - mix); }
    limiter.setThreshold(-0.3f); limiter.setRelease(*apvts.getRawParameterValue("limRel"));
    juce::dsp::AudioBlock<float> blk(buf);
    limiter.process(juce::dsp::ProcessContextReplacing<float>(blk));
    buf.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("post")->load()));
}

juce::AudioProcessorEditor* ClawpressorProcessor::createEditor() { return new ClawpressorEditor(*this); }
void ClawpressorProcessor::getStateInformation(juce::MemoryBlock& d)
{ if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d); }
void ClawpressorProcessor::setStateInformation(const void* d, int s)
{ if (auto xml = getXmlFromBinary(d, s)) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ClawpressorProcessor(); }
