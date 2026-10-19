#pragma once
#include "PluginProcessor.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>
#include <catui/SodaPop.h>

namespace claw
{
inline juce::Colour bandColour(int i)
{
    static const juce::uint32 c[] = { 0xff5aa8ff, 0xff58e08a, 0xfff2a23c, 0xffff7ac8 };
    return juce::Colour(c[i & 3]);
}

// A rotary knob with a name above and the value below.
struct Knob
{
    juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label l;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    void init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
              const juce::String& name, const juce::String& suffix);
    void place(juce::Rectangle<int> r);
};

// Spectrum-style band display: level bars per band, gain reduction, draggable crossover lines.
class BandView : public juce::Component
{
public:
    explicit BandView(ClawpressorProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    float xForHz(float hz) const;
    float hzForX(float x) const;
    ClawpressorProcessor& proc;
    int dragging = -1;
};

// Static compression curve of one band with the live input level as a dot.
class TransferView : public juce::Component
{
public:
    TransferView(ClawpressorProcessor& p, int band) : proc(p), b(band) {}
    void paint(juce::Graphics&) override;
    // Drag the white dot sideways for the threshold, the lower dot for the lift floor (upward compression).
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    float dbForX(float x) const;
    ClawpressorProcessor& proc; int b; int drag = -1;
};

// 16 step rhythmic gain pattern. Click to raise a step, right click to lower it.
class PatternView : public juce::Component
{
public:
    explicit PatternView(ClawpressorProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    ClawpressorProcessor& proc;
};

class BandPanel : public juce::Component
{
public:
    BandPanel(ClawpressorProcessor&, int band);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    ClawpressorProcessor& proc; int b;
    std::array<Knob, 10> knobs;
    juce::ToggleButton on { "On" }, mute { "M" }, solo { "S" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aOn, aMute, aSolo;
    TransferView curve;
};
}

class ClawpressorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ClawpressorEditor(ClawpressorProcessor&);
    ~ClawpressorEditor() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;   // on a wood cheek: back to 100 %
    static constexpr int baseW = 1100, baseH = 800, cheek = 24;
private:
    ClawpressorProcessor& proc;
    catui::CatLookAndFeel laf;
    catui::RenderGate gate { *this }; // pauses drawing when the window is hidden or in the background
    claw::BandView bandView;
    std::array<std::unique_ptr<claw::BandPanel>, ClawpressorProcessor::kBands> panels;
    claw::PatternView pattern;
    juce::ComboBox presets, look, speed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aLook, aSpeed;
    claw::Knob mix, limRel, post, strength;
    juce::ToggleButton clip { "Soft clip" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aClip;
    juce::TooltipWindow tooltip { this, 1000 };
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableBorderComponent border;
    int patLabelY = 600;
    juce::Label presetLabel { {}, "Preset" }, lookLabel { {}, "Lookahead" }, speedLabel { {}, "Pattern speed" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClawpressorEditor)
};
