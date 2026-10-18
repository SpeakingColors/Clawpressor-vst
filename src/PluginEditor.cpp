#include "PluginEditor.h"

ClawpressorEditor::ClawpressorEditor(ClawpressorProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);
    const char* ids[] = {"mix", "xo1", "xo2", "xo3", "limRel", "post"};
    const char* nm[]  = {"Dry/Wet", "X1", "X2", "X3", "Lim rel", "Post"};
    for (int i = 0; i < 6; ++i)
    {
        auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
        auto l = std::make_unique<juce::Label>(juce::String(), nm[i]);
        l->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*s); addAndMakeVisible(*l);
        att.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, ids[i], *s));
        knobs.push_back(std::move(s)); labels.push_back(std::move(l));
    }
    setSize(720, 300);
    gate.onFrame = [this] { repaint(); };
    gate.start(30);
}

void ClawpressorEditor::paint(juce::Graphics& g)
{
    g.fillAll(catui::Colours::panel());
    g.setColour(catui::Colours::accent());
    g.setFont(18.f);
    g.drawText("Clawpressor", 12, 8, 300, 24, juce::Justification::left);
    // simple gain reduction meters, one per band (animated from the audio thread values)
    for (int b = 0; b < ClawpressorProcessor::kBands; ++b)
    {
        const float gr = juce::jlimit(0.f, 24.f, proc.grMeter[(size_t) b].load());
        auto r = juce::Rectangle<float>(20.f + 170.f * (float) b, 50.f, 150.f, 12.f);
        g.setColour(catui::Colours::panelLight()); g.fillRect(r);
        g.setColour(catui::Colours::accent()); g.fillRect(r.withWidth(r.getWidth() * gr / 24.f));
    }
}

void ClawpressorEditor::resized()
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int x = 20 + (int) i * 110;
        knobs[i]->setBounds(x, 100, 90, 90);
        labels[i]->setBounds(x, 195, 90, 20);
    }
}
