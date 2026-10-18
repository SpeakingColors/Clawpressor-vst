#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Bands.h"

class ClawpressorProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kBands = 4;
    ClawpressorProcessor();
    void prepareToPlay(double sampleRate, int blockSize) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Clawpressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>, kBands> grMeter {};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    // Linkwitz-Riley split: 3 crossovers build 4 bands (low, low-mid, high-mid, high)
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> lowSplit, highSplit; // LP and HP per crossover
    std::array<BandComp, kBands> comp;
    juce::AudioBuffer<float> bandBuf[kBands], rest, dry;
    juce::dsp::Limiter<float> limiter;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClawpressorProcessor)
};
