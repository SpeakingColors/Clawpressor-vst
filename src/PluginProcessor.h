#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <claw/ClawEngine.h>
#include "Presets.h"

class ClawpressorProcessor : public juce::AudioProcessor
{
public:
    ClawpressorProcessor();
    ~ClawpressorProcessor() override;
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
    int getNumPrograms() override { return (int) factory.size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int i) override { return juce::isPositiveAndBelow(i, (int) factory.size()) ? factory[(size_t) i].name : juce::String(); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // presets: factory list, user presets are small XML files in Studio 87/Clawpressor/presets
    void applyValues(const claw::Values&, const juce::String& name);
    juce::File userDir() const;
    juce::StringArray userPresetNames() const;
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::String& name);
    bool deleteUserPreset(const juce::String& name);
    juce::String presetName() const { return apvts.state.getProperty("preset", factory.empty() ? juce::String() : factory[0].name).toString(); }
    const std::vector<claw::Preset>& factoryList() const { return factory; }
    bool tipsOn() const { return (bool) apvts.state.getProperty("tips", true); }
    void setTipsOn(bool v) { apvts.state.setProperty("tips", v, nullptr); }
    float get(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    claw::Engine engine;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void pullParams();
    struct Ptrs;
    std::unique_ptr<Ptrs> ptrs;
    std::vector<claw::Preset> factory = claw::factoryPresets();
    int program = 0;
    double fs = 44100.0, freeBeat = 0.0;
    std::vector<float> tmpR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClawpressorProcessor)
};
