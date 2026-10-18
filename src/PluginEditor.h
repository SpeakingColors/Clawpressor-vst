#pragma once
#include "PluginProcessor.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>

// Scaffold editor: the real layout follows docs/mockup/clawpressor.html.
class ClawpressorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ClawpressorEditor(ClawpressorProcessor&);
    ~ClawpressorEditor() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    ClawpressorProcessor& proc;
    catui::CatLookAndFeel laf;
    catui::RenderGate gate { *this }; // pauses drawing when the window is hidden or in the background
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> att;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClawpressorEditor)
};
