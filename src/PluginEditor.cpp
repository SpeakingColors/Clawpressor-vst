#include "PluginEditor.h"
#include <catui/CatLookAndFeel.h>

using namespace claw;

void Knob::init(juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
                const juce::String& name, const juce::String& suffix)
{
    s.setTextValueSuffix(suffix);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 15);
    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::FontOptions(12.f));
    parent.addAndMakeVisible(s); parent.addAndMakeVisible(l);
    a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, s);
}
void Knob::place(juce::Rectangle<int> r)
{
    l.setBounds(r.removeFromTop(14));
    s.setBounds(r);
}

static float rawParam(ClawpressorProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue(id)->load(); }

// ---------------- BandView
float BandView::xForHz(float hz) const { return (float) getWidth() * std::log(hz / 20.f) / std::log(1000.f); }
float BandView::hzForX(float x) const { return 20.f * std::pow(1000.f, x / (float) getWidth()); }

void BandView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1213));
    const float xo[3] = { rawParam(proc, "xo1"), rawParam(proc, "xo2"), rawParam(proc, "xo3") };
    const float edges[5] = { 20.f, xo[0], xo[1], xo[2], 20000.f };
    const float h = (float) getHeight();
    g.setFont(juce::FontOptions(11.f));
    for (float f : { 100.f, 1000.f, 10000.f })
    {
        g.setColour(juce::Colour(0xff223030)); g.drawVerticalLine((int) xForHz(f), 0.f, h);
        g.setColour(juce::Colour(0xff6b8279)); g.drawText(f >= 1000.f ? juce::String((int) (f / 1000.f)) + "k" : juce::String((int) f), (int) xForHz(f) + 3, (int) h - 14, 40, 12, juce::Justification::left);
    }
    for (int b = 0; b < 4; ++b)
    {
        const float x0 = xForHz(edges[b]), x1 = xForHz(edges[b + 1]);
        const float lv = proc.lvMeter[(size_t) b].load(), gr = proc.grMeter[(size_t) b].load();
        const float top = juce::jmap(juce::jlimit(-70.f, 0.f, lv), -70.f, 0.f, h, 6.f);
        const float topAfter = juce::jmap(juce::jlimit(-70.f, 0.f, lv - gr), -70.f, 0.f, h, 6.f);
        const bool active = rawParam(proc, "b" + juce::String(b) + "_on") > .5f;
        g.setColour(bandColour(b).withAlpha(0.2f)); g.fillRect(x0, top, x1 - x0, h - top);
        g.setColour(bandColour(b).withAlpha(active ? 0.65f : 0.25f)); g.fillRect(x0, topAfter, x1 - x0, h - topAfter);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.drawText(juce::String("GR ") + juce::String(gr, 1) + " dB", (int) x0 + 4, 4, 100, 14, juce::Justification::left);
    }
    g.setColour(juce::Colours::white);
    for (int i = 0; i < 3; ++i) g.fillRect(xForHz(xo[i]) - 1.f, 0.f, 3.f, h);
}

void BandView::mouseDown(const juce::MouseEvent& e)
{
    dragging = -1; float best = 12.f;
    for (int i = 0; i < 3; ++i)
    {
        const float d = std::abs(xForHz(rawParam(proc, "xo" + juce::String(i + 1))) - (float) e.x);
        if (d < best) { best = d; dragging = i; }
    }
    if (dragging >= 0) if (auto* p = proc.apvts.getParameter("xo" + juce::String(dragging + 1))) p->beginChangeGesture();
}
void BandView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    if (auto* p = proc.apvts.getParameter("xo" + juce::String(dragging + 1)))
    {
        const auto range = proc.apvts.getParameterRange("xo" + juce::String(dragging + 1));
        const float hz = range.snapToLegalValue(juce::jlimit(range.start, range.end, hzForX((float) e.x)));
        p->setValueNotifyingHost(p->convertTo0to1(hz));
    }
}
void BandView::mouseUp(const juce::MouseEvent&)
{
    if (dragging >= 0) if (auto* p = proc.apvts.getParameter("xo" + juce::String(dragging + 1))) p->endChangeGesture();
    dragging = -1;
}

// ---------------- TransferView
void TransferView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1213));
    const auto r = getLocalBounds().toFloat().reduced(2.f);
    const juce::String s = "b" + juce::String(b) + "_";
    BandComp c;
    c.thresholdDb = rawParam(proc, s + "th"); c.ratio = rawParam(proc, s + "ratio"); c.kneeDb = rawParam(proc, s + "knee"); c.boostDb = rawParam(proc, s + "boost");
    const float fl = rawParam(proc, s + "ufl"), ur = rawParam(proc, s + "uamt");
    auto pt = [&](float inDb, float outDb)
    { return juce::Point<float>(r.getX() + (inDb + 60.f) / 60.f * r.getWidth(), r.getBottom() - (juce::jlimit(-60.f, 0.f, outDb) + 60.f) / 60.f * r.getHeight()); };
    g.setColour(juce::Colour(0xff33403f)); g.drawLine({ pt(-60, -60), pt(0, 0) });
    juce::Path p;
    for (int x = 0; x <= 60; ++x) { const float in = -60.f + (float) x; const auto q = pt(in, c.curve(in) + c.boostDb + (in < c.thresholdDb ? claw::upwardLiftDb(in, fl, ur) : 0.f)); x ? p.lineTo(q) : p.startNewSubPath(q); }
    g.setColour(bandColour(b)); g.strokePath(p, juce::PathStrokeType(2.f));
    g.setFont(juce::FontOptions(9.f)); g.setColour(juce::Colour(0xff6b8279));
    for (int d = -60; d <= 0; d += 20) { g.drawText(juce::String(d), (int) pt((float) d, -60).x, getHeight() - 11, 22, 10, juce::Justification::left); }
    const auto th = pt(c.thresholdDb, c.curve(c.thresholdDb) + c.boostDb);
    g.setColour(juce::Colours::white); g.drawEllipse(th.x - 4.f, th.y - 4.f, 8.f, 8.f, 1.5f);
    if (ur > 1.04f) { const auto lf = pt(fl, fl); g.setColour(juce::Colour(0xffffd100)); g.drawEllipse(lf.x - 4.f, lf.y - 4.f, 8.f, 8.f, 1.5f); }
    const float lv = juce::jlimit(-60.f, 0.f, proc.lvMeter[(size_t) b].load());
    const auto d = pt(lv, c.curve(lv) + c.boostDb);
    g.setColour(juce::Colours::white); g.fillEllipse(d.x - 3.f, d.y - 3.f, 6.f, 6.f);
}

float TransferView::dbForX(float x) const { return juce::jlimit(-60.f, 0.f, -60.f + (x - 2.f) / ((float) getWidth() - 4.f) * 60.f); }
void TransferView::mouseDown(const juce::MouseEvent& e)
{
    const juce::String s = "b" + juce::String(b) + "_";
    const float x = (float) e.x, thx = 2.f + (rawParam(proc, s + "th") + 60.f) / 60.f * ((float) getWidth() - 4.f);
    const float flx = 2.f + (rawParam(proc, s + "ufl") + 60.f) / 60.f * ((float) getWidth() - 4.f);
    drag = std::abs(x - thx) <= std::abs(x - flx) ? 0 : 1;
    if (auto* p = proc.apvts.getParameter(s + (drag == 0 ? "th" : "ufl"))) p->beginChangeGesture();
    mouseDrag(e);
}
void TransferView::mouseDrag(const juce::MouseEvent& e)
{
    if (drag < 0) return;
    const juce::String id = "b" + juce::String(b) + "_" + (drag == 0 ? "th" : "ufl");
    if (auto* p = proc.apvts.getParameter(id))
    {
        const auto range = proc.apvts.getParameterRange(id);
        p->setValueNotifyingHost(p->convertTo0to1(range.snapToLegalValue(juce::jlimit(range.start, range.end, dbForX((float) e.x)))));
    }
}
void TransferView::mouseUp(const juce::MouseEvent&)
{
    if (drag < 0) return;
    if (auto* p = proc.apvts.getParameter("b" + juce::String(b) + "_" + (drag == 0 ? "th" : "ufl"))) p->endChangeGesture();
    drag = -1;
}

// ---------------- PatternView
void PatternView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1213));
    const float w = (float) getWidth() / (float) ClawpressorProcessor::kSteps, h = (float) getHeight();
    const int cur = proc.patStep.load();
    const bool live = rawParam(proc, "str") > 0.f;
    for (int s = 0; s < ClawpressorProcessor::kSteps; ++s)
    {
        const int lvl = (int) rawParam(proc, "pat" + juce::String(s));
        const float bh = 8.f + (h - 12.f) * (float) lvl / 3.f;
        g.setColour(live && s == cur ? juce::Colour(0xff58e08a) : juce::Colour(0xff4a5b57));
        g.fillRect(w * (float) s + 2.f, h - bh, w - 4.f, bh);
    }
}
void PatternView::mouseDown(const juce::MouseEvent& e)
{
    const int s = juce::jlimit(0, ClawpressorProcessor::kSteps - 1, (int) ((float) e.x / ((float) getWidth() / (float) ClawpressorProcessor::kSteps)));
    if (auto* p = proc.apvts.getParameter("pat" + juce::String(s)))
    {
        int v = (int) rawParam(proc, "pat" + juce::String(s));
        v = e.mods.isRightButtonDown() ? (v + 3) % 4 : (v + 1) % 4;
        p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1((float) v)); p->endChangeGesture();
    }
    repaint();
}

// ---------------- BandPanel
BandPanel::BandPanel(ClawpressorProcessor& p, int band) : proc(p), b(band), curve(p, band)
{
    const juce::String s = "b" + juce::String(b) + "_";
    knobs[0].init(*this, p.apvts, s + "th", "Thresh", " dB");
    knobs[1].init(*this, p.apvts, s + "ratio", "Ratio", ":1");
    knobs[2].init(*this, p.apvts, s + "knee", "Knee", " dB");
    knobs[3].init(*this, p.apvts, s + "att", "Attack", " ms");
    knobs[4].init(*this, p.apvts, s + "rel", "Release", " ms");
    knobs[5].init(*this, p.apvts, s + "boost", "Boost", " dB");
    knobs[6].init(*this, p.apvts, s + "ufl", "Lift floor", " dB");
    knobs[7].init(*this, p.apvts, s + "uamt", "Lift ratio", ":1");
    knobs[8].init(*this, p.apvts, s + "sat", "Saturation", " %");
    knobs[9].init(*this, p.apvts, s + "wid", "Width", " %");
    knobs[0].s.setTooltip("Level above which the band is compressed. Drag the white dot in the curve too.");
    knobs[6].s.setTooltip("Upward compression: signals quieter than this are lifted. Drag the yellow dot in the curve too.");
    knobs[7].s.setTooltip("How strongly quiet signals are lifted. 1:1 is off. 2:1 lifts a signal 10 dB under the floor by 5 dB.");
    knobs[8].s.setTooltip("Antialiased tanh saturation on this band, level compensated.");
    knobs[9].s.setTooltip("Stereo width of this band. 0 is mono (use it on the low band for a mono bass), 100 is unchanged.");
    for (auto* t : { &on, &mute, &solo }) addAndMakeVisible(*t);
    aOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, s + "on", on);
    aMute = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, s + "mute", mute);
    aSolo = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, s + "solo", solo);
    addAndMakeVisible(curve);
}
void BandPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff1e2528)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
    g.setColour(bandColour(b)); g.fillRect(0, 0, getWidth(), 3);
    static const char* nm[] = { "Low", "Low-mid", "High-mid", "High" };
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
    g.drawText(nm[b], 8, 6, 80, 18, juce::Justification::left);
    const float gr = juce::jlimit(0.f, 24.f, proc.grMeter[(size_t) b].load());
    const auto m = juce::Rectangle<float>(8.f, (float) getHeight() - 12.f, (float) getWidth() - 16.f, 6.f);
    g.setColour(juce::Colour(0xff0e1213)); g.fillRect(m);
    g.setColour(juce::Colour(0xfff2a23c)); g.fillRect(m.withWidth(m.getWidth() * gr / 24.f));
}
void BandPanel::resized()
{
    auto r = getLocalBounds().reduced(6);
    auto top = r.removeFromTop(22);
    solo.setBounds(top.removeFromRight(34)); mute.setBounds(top.removeFromRight(34)); on.setBounds(top.removeFromRight(40));
    const int kw = r.getWidth() / 3, kh = juce::jmax(50, (r.getHeight() - 110) / 4);
    for (int i = 0; i < 10; ++i) knobs[(size_t) i].place({ r.getX() + (i % 3) * kw, r.getY() + (i / 3) * kh, kw, kh });
    curve.setBounds(r.withTrimmedTop(4 * kh + 4).withTrimmedBottom(16));
}

// ---------------- Editor
ClawpressorEditor::ClawpressorEditor(ClawpressorProcessor& p)
    : AudioProcessorEditor(&p), proc(p), bandView(p), pattern(p), border(this, nullptr)
{
    setLookAndFeel(&laf);
    clip.setTooltip("Smooth knee clipper at the very end (ceiling just under full scale). Keeps peaks from ever clipping the converter.");
    addAndMakeVisible(clip);
    aClip = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "clip", clip);
    addAndMakeVisible(bandView);
    for (int b = 0; b < ClawpressorProcessor::kBands; ++b) { panels[(size_t) b] = std::make_unique<BandPanel>(p, b); addAndMakeVisible(*panels[(size_t) b]); }
    addAndMakeVisible(pattern);
    for (int i = 0; i < p.getNumPrograms(); ++i) presets.addItem(p.getProgramName(i), i + 1);
    presets.setSelectedId(p.getCurrentProgram() + 1, juce::dontSendNotification);
    presets.onChange = [this] { proc.setCurrentProgram(presets.getSelectedId() - 1); };
    addAndMakeVisible(presets);
    for (int i = 0; i < 5; ++i) look.addItem(ClawpressorProcessor::lookNames[i], i + 1);
    speed.addItemList({ "1/16", "1/8", "1/4" }, 1);
    aLook = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "look", look);
    aSpeed = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "speed", speed);
    addAndMakeVisible(look); addAndMakeVisible(speed);
    for (auto* l : { &presetLabel, &lookLabel, &speedLabel }) { addAndMakeVisible(*l); l->setFont(juce::FontOptions(12.f)); }
    mix.init(*this, p.apvts, "mix", "Dry/Wet", " %");
    limRel.init(*this, p.apvts, "limRel", "Limiter rel", " ms");
    post.init(*this, p.apvts, "post", "Post gain", " dB");
    strength.init(*this, p.apvts, "str", "Pattern", " %");
    setResizable(true, false);
    setResizeLimits(baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio((double) baseW / (double) baseH);
    border.setBorderThickness(juce::BorderSize<int>(6));
    border.setConstrainer(getConstrainer());
    addAndMakeVisible(border);
    setSize(baseW, baseH);
    gate.onFrame = [this]
    {
        bandView.repaint(); pattern.repaint();
        for (auto& pn : panels) pn->repaint();
    };
    gate.start(30);
}

void ClawpressorEditor::paint(juce::Graphics& g)
{
    g.fillAll(catui::Colours::panel());
    catui::drawWoodCheeks(g, getLocalBounds(), cheek);
    catui::drawCatHead(g, { (float) cheek + 10.f, 8.f, 34.f, 34.f }, catui::SodaPop::lilac());
    g.setColour(catui::Colours::accent());
    g.setFont(juce::FontOptions(22.f, juce::Font::bold));
    g.drawText("Clawpressor", cheek + 52, 6, 220, 26, juce::Justification::left);
    g.setColour(catui::Colours::dim()); g.setFont(juce::FontOptions(12.f));
    g.drawText("Studio 87 - four band compressor with lift, saturation and width", cheek + 52, 28, 420, 16, juce::Justification::left);
    g.drawText("Rhythmic gain pattern (click a step, right click to lower)", cheek + 10, patLabelY, 420, 16, juce::Justification::left);
}

void ClawpressorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.x < cheek || e.x > getWidth() - cheek) setSize(baseW, baseH);
}

void ClawpressorEditor::resized()
{
    border.setBounds(getLocalBounds());
    auto r = getLocalBounds().reduced(cheek + 8, 8);
    auto top = r.removeFromTop(44);
    presets.setBounds(top.removeFromRight(280).withSizeKeepingCentre(280, 28)); presetLabel.setBounds(top.removeFromRight(50));
    r.removeFromTop(4);
    bandView.setBounds(r.removeFromTop(juce::roundToInt(r.getHeight() * 0.2f)));
    r.removeFromTop(8);
    auto row = r.removeFromTop(juce::roundToInt(r.getHeight() * 0.67f));
    const int w = row.getWidth() / 4;
    for (int b = 0; b < 4; ++b) panels[(size_t) b]->setBounds(row.removeFromLeft(w).reduced(3));
    r.removeFromTop(6);
    patLabelY = r.getY();
    auto bottom = r;
    auto left = bottom.removeFromLeft(juce::roundToInt(bottom.getWidth() * 0.46f));
    left.removeFromTop(20);
    pattern.setBounds(left.removeFromTop(juce::jmax(40, left.getHeight() - 34)).reduced(0, 2));
    auto ctl = left;
    speedLabel.setBounds(ctl.removeFromLeft(90).withHeight(20)); speed.setBounds(ctl.removeFromLeft(80).withHeight(24));
    ctl.removeFromLeft(14);
    lookLabel.setBounds(ctl.removeFromLeft(70).withHeight(20)); look.setBounds(ctl.removeFromLeft(90).withHeight(24));
    ctl.removeFromLeft(14);
    clip.setBounds(ctl.removeFromLeft(100).withHeight(24));
    bottom.removeFromLeft(20);
    const int kw = bottom.getWidth() / 4;
    mix.place(bottom.removeFromLeft(kw).withTrimmedBottom(10)); strength.place(bottom.removeFromLeft(kw).withTrimmedBottom(10));
    limRel.place(bottom.removeFromLeft(kw).withTrimmedBottom(10)); post.place(bottom.withTrimmedBottom(10));
}
