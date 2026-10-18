#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Bands.h"

class ClawpressorProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kBands = 4, kSteps = 16;
    ClawpressorProcessor();
    void prepareToPlay(double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Clawpressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    // read by the editor (meters), written by the audio thread
    std::array<std::atomic<float>, kBands> grMeter {}, lvMeter {};
    std::atomic<int> patStep { 0 };
    std::atomic<float> masterGr { 0.f };

    static const char* lookNames[];
    static constexpr float lookMs[5] = { 0.f, 1.f, 2.5f, 5.f, 7.5f };
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void applyPreset(int);
    float get(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }

    // Phase-aligned 4-band Linkwitz-Riley split. LP + HP of one LR4 filter is an allpass,
    // so each lower band passes through allpass copies of the higher crossovers.
    juce::dsp::LinkwitzRileyFilter<float> s1, s2, s3, a2a, a3a, a3b;
    std::array<BandComp, kBands> comp;
    DryDelay dryDelay;
    std::vector<float> bandData[kBands][2], dryData[2];
    juce::dsp::Limiter<float> limiter;
    double fs = 44100.0;
    int maxBlock = 0, program = 0, lastLatency = -1;
    double freePpq = 0.0;
    float patGain = 1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClawpressorProcessor)
};
