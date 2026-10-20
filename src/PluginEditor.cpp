#include "PluginEditor.h"
#include <cmath>
#include <cstdlib>

using namespace clawui;
using juce::Graphics;
using juce::Rectangle;

namespace
{
constexpr float kPi = 3.14159265358979f;
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float dbOf(float lin) { return lin > 3.2e-5f ? 20.f * std::log10(lin) : -90.f; }

// text anchored at a point: h = 0 left, 1 centre, 2 right; v = 0 top, 1 middle, 2 bottom
void T(Graphics& g, const String& s, float x, float y, int h, int v, const juce::Font& f, Colour c)
{
    g.setFont(f); g.setColour(c);
    const float W = 600.f, H = 24.f;
    const float rx = h == 0 ? x : h == 1 ? x - W / 2 : x - W;
    const float ry = v == 0 ? y : v == 1 ? y - H / 2 : y - H;
    const int j = (h == 0 ? juce::Justification::left : h == 1 ? juce::Justification::horizontallyCentred : juce::Justification::right)
                | (v == 0 ? juce::Justification::top : v == 1 ? juce::Justification::verticallyCentred : juce::Justification::bottom);
    g.drawText(s, Rectangle<float>(rx, ry, W, H), juce::Justification(j), false);
}
}

namespace clawui
{
String fmtDb(double v) { return String(v > 0.05 ? "+" : "") + String(v, 1) + " dB"; }
String fmtMs(double v) { return v < 10 ? String(v, 1) + " ms" : v < 1000 ? String(juce::roundToInt(v)) + " ms" : String(v / 1000.0, 2) + " s"; }
String fmtHz(double v)
{
    if (v < 1000) return String(juce::roundToInt(v)) + " Hz";
    String s = String(v / 1000.0, v >= 10000 ? 1 : 2);
    if (s.contains(".")) { s = s.trimCharactersAtEnd("0"); s = s.trimCharactersAtEnd("."); }
    return s + " kHz";
}
String fmtPct(double v) { return String(juce::roundToInt(v)) + " %"; }

// ------------------------------------------------------------------ look
Look::Look()
{
    using juce::PopupMenu; using juce::ComboBox; using juce::Label; using juce::TextEditor;
    setColour(PopupMenu::backgroundColourId, cField); setColour(PopupMenu::textColourId, cInk);
    setColour(PopupMenu::highlightedBackgroundColourId, cPanel2); setColour(PopupMenu::highlightedTextColourId, cMint);
    setColour(PopupMenu::headerTextColourId, cDim);
    setColour(ComboBox::backgroundColourId, cField); setColour(ComboBox::outlineColourId, cLine); setColour(ComboBox::textColourId, cInk);
    setColour(ComboBox::arrowColourId, cInk);
    setColour(Label::textColourId, cInk); setColour(Label::outlineWhenEditingColourId, cMint); setColour(Label::backgroundWhenEditingColourId, cField);
    setColour(TextEditor::backgroundColourId, cField); setColour(TextEditor::textColourId, cInk); setColour(TextEditor::outlineColourId, cLine);
    setColour(TextEditor::focusedOutlineColourId, cMint); setColour(TextEditor::highlightColourId, cMint.withAlpha(0.35f));
    setColour(juce::CaretComponent::caretColourId, cMint);
}

void Look::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto c = Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
    const float sc = (float) s.getProperties().getWithDefault("sc", 0.88);
    const Colour col((juce::uint32) (juce::int64) s.getProperties().getWithDefault("col", (juce::int64) cMint.getARGB()));
    const float R = 22.f * sc, ar = 23.f * sc, th = 3.f * sc, ang = a0 + pos * (a1 - a0);
    juce::Path trk; trk.addCentredArc(c.x, c.y, ar, ar, 0.f, a0, a1, true);
    g.setColour(Colour(0xff33353d)); g.strokePath(trk, juce::PathStrokeType(th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    float from = a0;
    const bool bip = s.getProperties().contains("mid");
    if (bip) from = a0 + (float) s.valueToProportionOfLength((double) (float) (double) s.getProperties()["mid"]) * (a1 - a0);
    const float lo = juce::jmin(from, ang), hi = juce::jmax(from, ang);
    if (bip ? (hi - lo > 0.012f) : pos > 0.002f)
    {
        juce::Path v; v.addCentredArc(c.x, c.y, ar, ar, 0.f, bip ? lo : a0, bip ? hi : ang, true);
        g.setColour(col); g.strokePath(v, juce::PathStrokeType(th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ColourGradient gr(Colour(0xff33363f), c.x - 0.3f * R, c.y - 0.4f * R, Colour(0xff23252c), c.x + 0.8f * R, c.y + 0.8f * R, true);
    g.setGradientFill(gr); g.fillEllipse(c.x - R, c.y - R, 2 * R, 2 * R);
    g.setColour(Colour(0xff3a3d48)); g.drawEllipse(c.x - R + 0.5f, c.y - R + 0.5f, 2 * R - 1.f, 2 * R - 1.f, 1.f);
    juce::Path tick; tick.addRoundedRectangle(-1.f * sc, -19.f * sc, 2.f * sc, 15.f * sc, 1.f * sc);
    g.setColour(cInk); g.fillPath(tick, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
}

void Look::drawButtonBackground(Graphics& g, juce::Button& b, const Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const auto id = b.getComponentID();
    if (id == "pname") { g.setColour(cField); g.fillRoundedRectangle(r, 3.f); g.setColour(hi ? Colour(0xff4a4d58) : cLine); g.drawRoundedRectangle(r, 3.f, 1.f); return; }
    const bool on = b.getToggleState();
    Colour bg = on ? cPressed : cPanel2, bd = on ? cMint : cLine;
    if (id.startsWith("bsel") && on) { bg = bd = bandColour(id.getTrailingIntValue() - 1); }
    else if (hi) bd = on ? cMint : Colour(0xff4a4d58);
    if (down) bg = bg.brighter(0.12f);
    g.setColour(bg); g.fillRoundedRectangle(r, 3.f);
    g.setColour(bd); g.drawRoundedRectangle(r, 3.f, 1.f);
}

void Look::drawButtonText(Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto id = b.getComponentID();
    const bool on = b.getToggleState();
    const float fs = b.getProperties().contains("fs") ? (float) (double) b.getProperties()["fs"] : juce::jlimit(10.f, 15.f, b.getHeight() * 0.46f);
    if (id == "pname")
    {
        g.setColour(cInk); g.setFont(monoFont(14.f));
        g.drawText(b.getButtonText(), b.getLocalBounds().withTrimmedLeft(12).withTrimmedRight(26), juce::Justification::centredLeft, true);
        g.setColour(cMint); g.setFont(uiFont(12.f));
        g.drawText(juce::CharPointer_UTF8("\xe2\x96\xbe"), b.getLocalBounds().withTrimmedRight(10), juce::Justification::centredRight, false);
        return;
    }
    Colour tc = on ? cMint : cInk;
    if (id.startsWith("bsel") && on) tc = (id.getTrailingIntValue() >= 3) ? Colour(0xffffffff) : Colour(0xff111111);
    g.setColour(tc); g.setFont(uiFont(fs, true, 0.08f));
    g.drawText(b.getButtonText().toUpperCase(), b.getLocalBounds().reduced(2, 0), juce::Justification::centred, false);
}

void Look::drawComboBox(Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
    g.setColour(cField); g.fillRoundedRectangle(r, 3.f);
    g.setColour(box.hasKeyboardFocus(false) ? cMint : cLine); g.drawRoundedRectangle(r, 3.f, 1.f);
    juce::Path p; const float cx = (float) w - 13.f, cy = (float) h * 0.5f;
    p.startNewSubPath(cx - 4.f, cy - 2.f); p.lineTo(cx, cy + 2.f); p.lineTo(cx + 4.f, cy - 2.f);
    g.setColour(box.isEnabled() ? cInk : cDim); g.strokePath(p, juce::PathStrokeType(1.6f));
}
void Look::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{ label.setBounds(8, 1, box.getWidth() - 26, box.getHeight() - 2); label.setFont(getComboBoxFont(box)); }

static juce::TextLayout tipLayout(const String& text, float maxW)
{
    juce::AttributedString as; as.setJustification(juce::Justification::topLeft);
    const int nl = text.indexOfChar('\n');
    if (nl > 0) { as.append(text.substring(0, nl).toUpperCase() + "\n", uiFont(12.f, true, 0.14f), cMint); as.append(text.substring(nl + 1), uiFont(13.f, false, 0.02f), cInk); }
    else as.append(text, uiFont(13.f, false, 0.02f), cInk);
    juce::TextLayout l; l.createLayout(as, maxW); return l;
}
Rectangle<int> Look::getTooltipBounds(const String& text, juce::Point<int> pos, Rectangle<int> parent)
{
    auto l = tipLayout(text, 240.f);
    const int w = (int) std::ceil(l.getWidth()) + 22, h = (int) std::ceil(l.getHeight()) + 16;
    return Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - w - 12 : pos.x + 14, pos.y + 18, w, h).constrainedWithin(parent.reduced(4));
}
void Look::drawTooltip(Graphics& g, const String& text, int w, int h)
{
    auto r = Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
    g.setColour(cField); g.fillRoundedRectangle(r, 4.f);
    g.setColour(Colour(0xff4a4d58)); g.drawRoundedRectangle(r, 4.f, 1.f);
    tipLayout(text, 240.f).draw(g, Rectangle<float>(11.f, 8.f, (float) w - 22.f, (float) h - 16.f));
}

// ------------------------------------------------------------------ shared state
Shared::Shared(ClawpressorProcessor& p) : proc(p), specPre(2049, 0.f), specPost(2049, 0.f), win(4096), work(8192)
{
    for (int i = 0; i < 4096; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos(2.f * kPi * (float) i / 4096.f);
    for (auto& h : hist) h.reserve(1300);
}
void Shared::setParam(const String& id, float v)
{
    if (auto* p = proc.apvts.getParameter(id))
    {
        const auto r = proc.apvts.getParameterRange(id);
        p->setValueNotifyingHost(p->convertTo0to1(clampf(v, r.start, r.end)));
    }
}
void Shared::gesture(const String& id, bool begin)
{
    if (auto* p = proc.apvts.getParameter(id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); }
}
void Shared::update(float dt)
{
    auto& m = proc.engine.meters;
    const float rel = 24.f * dt;
    float* mh[4] = { &inL, &inR, &outL, &outR }; float* hh[4] = { &hInL, &hInR, &hOutL, &hOutR };
    const float raw[4] = { m.inL.exchange(0.f), m.inR.exchange(0.f), m.outL.exchange(0.f), m.outR.exchange(0.f) };
    for (int i = 0; i < 4; ++i)
    {
        const float v = dbOf(raw[i]);
        *mh[i] = v > *mh[i] ? v : juce::jmax(v, *mh[i] - rel);
        *hh[i] = v > *hh[i] ? v : juce::jmax(-90.f, *hh[i] - 6.f * dt);
    }
    const float acc = m.rmsAcc.exchange(0.f); const int N = m.rmsN.exchange(0);
    const float r = N > 0 ? 10.f * std::log10(acc / (float) N + 1e-10f) : -90.f;
    rms = r > rms ? r : juce::jmax(r, rms - 6.f * dt);
    if (inL > -0.2f || outL > -0.2f || outR > -0.2f) clipT = 1.5f;
    clipT = juce::jmax(0.f, clipT - dt);
    const float pk = juce::jmax(hOutL, hOutR), cr = pk - rms;
    if (std::isfinite(cr) && pk > -60.f) crest += (cr - crest) * juce::jmin(1.f, dt * 1.5f);
    dens = clampf(1.f - (crest - 5.f) / 15.f, 0.f, 1.f);
    for (int b = 0; b < 4; ++b)
    {
        const float g1 = m.gr[(size_t) b].exchange(0.f), g2 = m.lgr[(size_t) b].exchange(0.f), lv = dbOf(m.lvl[(size_t) b].exchange(0.f));
        gr[b] = g1 > gr[b] ? g1 : juce::jmax(g1, gr[b] - 30.f * dt);
        lgr[b] = g2 > lgr[b] ? g2 : juce::jmax(g2, lgr[b] - 30.f * dt);
        lvl[b] = lv > lvl[b] ? lv : juce::jmax(lv, lvl[b] - 24.f * dt);
    }
    const float g3 = m.mgr.exchange(0.f); mgr = g3 > mgr ? g3 : juce::jmax(g3, mgr - 30.f * dt);
    step = m.step.load(); active = juce::jmax(inL, inR) > -80.f;
    for (int k = 0; k < 5; ++k)
    {
        auto& h = hist[k]; h.push_back(k < 4 ? gr[k] : mgr);
        const size_t cap = (size_t) (31.f / juce::jmax(0.01f, dt));
        if (h.size() > cap) h.erase(h.begin(), h.begin() + (long) (h.size() - cap));
    }
}
void Shared::updateSpectrum()
{
    auto& e = proc.engine; const int idx = e.scopeIdx.load(std::memory_order_acquire), nb = this->nb();
    for (int pass = 0; pass < 2; ++pass)
    {
        std::fill(work.begin(), work.end(), 0.f);
        for (int j = 0; j < 4096; ++j)
        {
            const size_t s = (size_t) ((idx - 4096 + j) & (claw::Engine::kScope - 1));
            float v = 0.f;
            for (int b = 0; b < nb; ++b) v += pass == 0 ? e.scopeA[(size_t) b][s] : e.scopeB[(size_t) b][s];
            work[(size_t) j] = v * win[(size_t) j];
        }
        fft.performFrequencyOnlyForwardTransform(work.data());
        auto& dst = pass == 0 ? specPre : specPost;
        for (int k = 0; k <= 2048; ++k) dst[(size_t) k] = 0.75f * dst[(size_t) k] + 0.25f * (work[(size_t) k] * 4.f / 4096.f);
    }
}

// ------------------------------------------------------------------ knob, toggle, select
Knob::Knob(Shared& sh_, const String& id_, const String& label, std::function<String(double)> f, Colour arc, float mid, const String& tip, bool side_, float scale_)
    : sh(sh_), id(id_), name(label), fmt(std::move(f)), side(side_), scale(scale_)
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*>(sh.proc.apvts.getParameter(id));
    jassert(p != nullptr);
    s.setRotaryParameters(1.25f * kPi, 2.75f * kPi, true);
    s.setMouseDragSensitivity(170);
    s.getProperties().set("col", (juce::int64) arc.getARGB());
    s.getProperties().set("sc", (double) (side ? 0.72 : scale));
    if (mid > kNoMid * 0.5f) s.getProperties().set("mid", (double) mid);
    addAndMakeVisible(s);
    att = std::make_unique<juce::SliderParameterAttachment>(*p, s, nullptr);
    s.setDoubleClickReturnValue(true, (double) p->convertFrom0to1(p->getDefaultValue()));
    s.textFromValueFunction = [this](double v) { return fmt(v); };
    val.setJustificationType(juce::Justification::centred);
    val.setFont(monoFont(12.f)); val.setColour(juce::Label::textColourId, cMint);
    val.setEditable(false, true, false);
    val.onTextChange = [this]
    {
        auto t = val.getText().trim(); double v = 0.0;
        if (t.containsIgnoreCase("inf")) v = s.getMaximum();
        else if (t.containsAnyOf("0123456789"))
        {
            v = t.retainCharacters("0123456789.-+").getDoubleValue();
            if (t.endsWithIgnoreCase("s") && !t.endsWithIgnoreCase("ms")) v *= 1000.0;
        }
        else { refresh(); return; }
        s.setValue(juce::jlimit(s.getMinimum(), s.getMaximum(), v), juce::sendNotificationSync); refresh();
    };
    addAndMakeVisible(val);
    s.onValueChange = [this] { if (!val.isBeingEdited()) refresh(); };
    setTip(tip);
    refresh();
}
void Knob::refresh() { val.setText(fmt(s.getValue()), juce::dontSendNotification); }
void Knob::setTip(const String& t)
{
    tipText = name + "\n" + t + " Double-click resets.";
    for (juce::Component* c : { (juce::Component*) &s, (juce::Component*) &val }) c->getProperties().set("tip", tipText);
}
void Knob::resized()
{
    auto r = getLocalBounds();
    if (side)
    {
        s.setBounds(r.removeFromLeft(40).withSizeKeepingCentre(40, 40)); r.removeFromLeft(4);
        val.setBounds(r.removeFromBottom(r.getHeight() / 2 + 1).withTrimmedTop(1));
    }
    else
    {
        s.setBounds(r.removeFromTop(44).withSizeKeepingCentre(juce::jmin(60, r.getWidth()), 44));
        r.removeFromTop(11);
        val.setBounds(r.removeFromTop(13));
    }
}
void Knob::paint(Graphics& g)
{
    if (side) T(g, name.toUpperCase(), 44.f, (float) getHeight() / 2 - 5.f, 0, 2, uiFont(11.f, false, 0.14f), cDim);
    else T(g, name.toUpperCase(), (float) getWidth() / 2, 44.f, 1, 0, uiFont(11.f, false, 0.14f), cDim);
}

Tog::Tog(Shared& sh, const String& id, const String& text, const String& tip) : juce::TextButton(text)
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*>(sh.proc.apvts.getParameter(id));
    setClickingTogglesState(true);
    att = std::make_unique<juce::ButtonParameterAttachment>(*p, *this, nullptr);
    getProperties().set("fs", 11.5);
    getProperties().set("tip", text + "\n" + tip);
}

Sel::Sel(Shared& sh, const String& id, const String& label, const String& tip) : name(label)
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*>(sh.proc.apvts.getParameter(id));
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(p)) box.addItemList(c->choices, 1);
    addAndMakeVisible(box);
    att = std::make_unique<juce::ComboBoxParameterAttachment>(*p, box, nullptr);
    box.getProperties().set("tip", label + "\n" + tip);
}
void Sel::resized() { auto r = getLocalBounds(); r.removeFromTop(14); box.setBounds(r.removeFromTop(26)); }
void Sel::paint(Graphics& g) { T(g, name.toUpperCase(), 0.f, 0.f, 0, 0, uiFont(11.f, false, 0.18f), cDim); }

// ------------------------------------------------------------------ band strip
void GrBar::paint(Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.setColour(cDisp); g.fillRoundedRectangle(0, 0, w, h, 3.f); g.setColour(cLine); g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 3.f, 1.f);
    const float gr = clampf(sh.gr[band] / 20.f, 0.f, 1.f), lg = clampf(sh.lgr[band] / 20.f, 0.f, 1.f);
    g.setColour(bandColour(band)); g.fillRect(w - w * gr, 1.f, w * gr, h / 2 - 1.f);
    g.setColour(Colour(0xccffffff)); g.fillRect(w - w * lg, h / 2, w * lg, h / 2 - 1.f);
    T(g, "GR -" + String(sh.gr[band], 1) + "  LIM -" + String(sh.lgr[band], 1), 4.f, h / 2 + 0.5f, 0, 1, monoFont(9.f), cInk);
}

BandStrip::BandStrip(Shared& s, int band) : grBar(s, band), sh(s), b(band)
{
    const String p = "b" + String(band + 1) + "_";
    const Colour c = bandColour(band);
    auto K = [&](const char* id, const char* label, std::function<String(double)> f, float mid, const char* tip)
    { knobs.push_back(std::make_unique<Knob>(sh, p + id, label, f, c, mid, tip)); addAndMakeVisible(*knobs.back()); };
    auto ratio = [](double v) { return v >= 19.95 ? String("inf:1") : String(v, 1) + ":1"; };
    auto rel = [this, p](double v) { return sh.param(p + "auto") > 0.5f ? String("Auto") : fmtMs(v); };
    K("in", "Input", fmtDb, 0.f, "Level into this band's compressor.");
    K("thr", "Thresh", fmtDb, kNoMid, "Level above which the band is compressed. Also draggable on the Curve view.");
    K("ratio", "Ratio", ratio, kNoMid, "How strongly the signal above the threshold is reduced. 'inf' makes it a limiter. Also draggable on the Curve view.");
    K("att", "Attack", fmtMs, kNoMid, "How fast the compressor reacts. Short grabs transients, long lets punch through.");
    K("rel", "Release", rel, kNoMid, "How fast the gain comes back. With Auto on, it follows the gain reduction: more reduction, slower release.");
    K("boost", "Boost", fmtDb, kNoMid, "Extra makeup gain after the compressor, on top of the automatic gain.");
    K("lthr", "Limit", fmtDb, kNoMid, "Ceiling of this band's limiter (ratio infinity). The limiter looks ahead 64 samples through the master delay, so it is fast without clicks.");
    K("lrel", "Lim rel", fmtMs, kNoMid, "Release of this band's limiter.");
    K("out", "Output", fmtDb, 0.f, "Level of this band after the limiter. Use it to rebalance the bands.");
    K("sat", "Sat", fmtPct, kNoMid, "Soft saturation of this band after the limiter (antialiased tanh). Drive and mix rise together. Warms bass and thickens leads.");
    K("wid", "Width", fmtPct, 100.f, "Stereo width of this band (mid/side). 0 makes the band mono: set the low band to 0 for a mono bass. 100 is unchanged.");
    auto Tg = [&](const char* id, const char* text, const char* tip) { togs.push_back(std::make_unique<Tog>(sh, p + id, text, tip)); addAndMakeVisible(*togs.back()); };
    Tg("solo", "S", "Solo: hear only this band (and any other soloed band).");
    Tg("mute", "M", "Mute: silence this band.");
    Tg("on", "Comp", "Turns this band's compressor on or off. The band still passes through.");
    Tg("auto", "Auto rel", "Release follows the gain reduction. The Release knob is ignored.");
    Tg("soft", "Soft knee", "Soft knee eases into compression around the threshold. Off is a hard knee.");
    Tg("lon", "Limiter", "Turns this band's limiter on or off.");
    for (int i = 0; i < 2; ++i) togs[(size_t) i]->getProperties().set("fs", 11.0);
    addAndMakeVisible(grBar);
}
void BandStrip::paint(Graphics& g)
{
    const auto c = bandColour(b); const float w = (float) getWidth(), h = (float) getHeight();
    g.setColour(cPanel2); g.fillRoundedRectangle(0, 0, w, h, 4.f);
    g.setColour(cLine); g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 4.f, 1.f);
    g.setColour(c); g.fillRect(1.f, 0.f, w - 2.f, 3.f);
    T(g, String(bandName(b)).toUpperCase(), 8.f, 17.f, 0, 1, uiFont(15.f, true, 0.14f), c);
    const int nb = sh.nb(); const float xs[5] = { 20.f, sh.param("x1"), sh.param("x2"), sh.param("x3"), 20000.f };
    auto k = [](float v) { return v >= 1000.f ? String(std::round(v / 100.f) / 10.f, 1).trimCharactersAtEnd("0").trimCharactersAtEnd(".") + "k" : String(juce::roundToInt(v)); };
    const String rng = b < nb ? k(xs[b]) + " - " + k(b == nb - 1 ? 20000.f : xs[b + 1]) + " Hz" : String("off");
    T(g, rng, 100.f, 17.f, 0, 1, monoFont(10.5f), cDim);
    g.setColour(cLine); g.drawHorizontalLine(180, 6.f, w - 6.f);
    T(g, "LIMITER", 6.f, 183.f, 0, 0, uiFont(10.f, false, 0.2f), cDim);
}
void BandStrip::resized()
{
    const int W = getWidth(), cw = (W - 12) / 3;
    togs[1]->setBounds(W - 6 - 24, 6, 24, 22); togs[0]->setBounds(W - 6 - 24 - 3 - 24, 6, 24, 22);
    grBar.setBounds(6, 32, W - 12, 15);
    const int rowY[4] = { 52, 116, 198, 262 };
    for (int i = 0; i < 11; ++i)
    {
        int row, col;
        if (i < 6) { row = i / 3; col = i % 3; } else { row = 2 + (i - 6) / 3; col = (i - 6) % 3; }
        knobs[(size_t) i]->setBounds(6 + col * cw, rowY[row], cw, 64);
    }
    togs[2]->setBounds(6, 332, 52, 22); togs[3]->setBounds(62, 332, 74, 22); togs[4]->setBounds(140, 332, 84, 22);
    togs[5]->setBounds(6, 358, 62, 22);
}
void BandStrip::tick() { knobs[4]->refresh(); }

// ------------------------------------------------------------------ display (bands, curve, scope, GR history, pattern)
namespace
{
float fxHz(float f, float w) { return std::log(f / 20.f) / std::log(1000.f) * w; }
float xHz(float x, float w) { return 20.f * std::pow(1000.f, x / w); }
float yDb(float d, float h, float top) { return top + (clampf(d, -90.f, 0.f)) / -90.f * (h - top - 22.f); }

void drawGrid(Graphics& g, float w, float h, float top)
{
    static const float fl[] = { 20, 30, 50, 70, 100, 200, 300, 500, 700, 1000, 2000, 3000, 5000, 7000, 10000, 20000 };
    for (float f : fl)
    {
        const float x = std::round(fxHz(f, w)) + 0.5f;
        g.setColour((f == 100 || f == 1000 || f == 10000) ? Colour(0xff25302f) : Colour(0xff161f1e)); g.drawLine(x, top, x, h - 22.f, 1.f);
        if (f == 20 || f == 50 || f == 100 || f == 200 || f == 500 || f == 1000 || f == 2000 || f == 5000 || f == 10000 || f == 20000)
            T(g, f >= 1000 ? String((int) (f / 1000)) + "k" : String((int) f), x, h - 17.f, f == 20 ? 0 : f == 20000 ? 2 : 1, 0, monoFont(11.f), cAxisX);
    }
    for (int d = 0; d >= -84; d -= 12)
    {
        const float y = std::round(yDb((float) d, h, top)) + 0.5f;
        g.setColour(cGridLine); g.drawLine(0.f, y, w, y, 1.f);
        T(g, String(d) + " dB", 4.f, y - 12.f < top ? y + 2.f : y - 12.f, 0, 0, monoFont(11.f), cAxisY);
    }
}
double effR(float v, bool whole) { double R = v; if (whole) R = std::max(1.0, std::round(R)); return R >= 19.95 ? 1e9 : R; }
double outAt(double thr, double R, double kw, double x)
{
    const double ov = x - thr;
    if (kw > 0 && 2 * ov > -kw && 2 * ov < kw) { const double t = ov + kw / 2; return x - (1 - 1 / R) * t * t / (2 * kw); }
    return ov > 0 ? thr + ov / R : x;
}
double outAtU(double thr, double R, double kw, double x, double fl, double ur)
{ double y = outAt(thr, R, kw, x); if (ur > 1.04 && x < fl) y = fl + (x - fl) / ur; return y; }
}

void DisplayView::paint(Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.setColour(cDisp); g.fillRect(0.f, 0.f, w, h);
    switch (sh.view) { case 0: drawBands(g, w, h); break; case 1: drawCurve(g, w, h); break; case 2: drawScope(g, w, h); break; case 3: drawGR(g, w, h); break; default: drawPat(g, w, h); }
    g.setColour(cLine); g.drawRect(0.f, 0.f, w, h, 1.f);
}

void DisplayView::drawBands(Graphics& g, float w, float h)
{
    const float top = 26.f; const int nb = sh.nb();
    drawGrid(g, w, h, top);
    const float xs[5] = { 20.f, sh.param("x1"), sh.param("x2"), sh.param("x3"), 20000.f };
    struct R { float a, b; int i; }; std::vector<R> bn;
    for (int b = 0; b < nb; ++b) bn.push_back({ fxHz(xs[b], w), b == nb - 1 ? w : fxHz(xs[b + 1], w), b });
    for (auto& r : bn) { g.setColour(bandColour(r.i).withAlpha(0.07f)); g.fillRect(r.a, top, r.b - r.a, h - 22.f - top); }
    {   // spectrum: before (grey dashed) and after (band colour)
        const double sr = sh.proc.getSampleRate() > 0 ? sh.proc.getSampleRate() : 44100.0; const float bh = (float) (sr / 4096.0);
        const int nc = (int) std::ceil(w / 2.f) + 1; std::vector<float> pre((size_t) nc), post((size_t) nc);
        for (int i = 0; i < nc; ++i)
        {
            const float f0 = xHz((float) (i * 2 - 1), w), f1 = xHz((float) (i * 2 + 1), w);
            const int b0 = std::max(1, (int) std::floor(f0 / bh)), b1 = std::min(2048, std::max(b0, (int) std::ceil(f1 / bh)));
            float m1 = 1e-9f, m2 = 1e-9f;
            for (int k = b0; k <= b1; ++k) { m1 = std::max(m1, sh.specPre[(size_t) k]); m2 = std::max(m2, sh.specPost[(size_t) k]); }
            const float tilt = 3.f * std::log2(xHz((float) (i * 2), w) / 1000.f);
            pre[(size_t) i] = 20.f * std::log10(m1) + tilt; post[(size_t) i] = 20.f * std::log10(m2) + tilt;
        }
        juce::Path pp;
        for (int i = 0; i < nc; ++i) { const float y = yDb(pre[(size_t) i], h, top); if (i == 0) pp.startNewSubPath(0.f, y); else pp.lineTo((float) i * 2.f, y); }
        const float dash[2] = { 4.f, 3.f }; juce::Path dp; juce::PathStrokeType(1.2f).createDashedStroke(dp, pp, dash, 2);
        g.setColour(Colour(0xff9a9fb0)); g.fillPath(dp);
        for (auto& r : bn)
        {
            g.saveState(); g.reduceClipRegion(Rectangle<float>(r.a, top, r.b - r.a, h - 22.f - top).toNearestInt());
            juce::Path fill, line;
            fill.startNewSubPath(0.f, h - 22.f);
            for (int i = 0; i < nc; ++i) { const float y = yDb(post[(size_t) i], h, top); fill.lineTo((float) i * 2.f, y); if (i == 0) line.startNewSubPath(0.f, y); else line.lineTo((float) i * 2.f, y); }
            fill.lineTo(w, h - 22.f); fill.closeSubPath();
            g.setColour(bandColour(r.i).withAlpha(0.4f)); g.fillPath(fill);
            g.setColour(bandColour(r.i)); g.strokePath(line, juce::PathStrokeType(1.8f)); g.restoreState();
        }
    }
    for (auto& r : bn)
    {
        const Colour col = bandColour(r.i); const float cx = (r.a + r.b) / 2; const int b = r.i;
        g.setColour(col); g.fillRect(r.a, top - 4.f, r.b - r.a, 4.f);
        T(g, String(bandName(b)).toUpperCase(), cx, top + 12.f, 1, 1, uiFont(13.f, true, 0.08f), col);
        const float mx = juce::jmin(120.f, (h - top - 22.f) * 0.4f), bh1 = clampf(sh.gr[b] / 20.f, 0.f, 1.f) * mx, bh2 = clampf(sh.lgr[b] / 20.f, 0.f, 1.f) * mx;
        g.setColour(col.withAlpha(0.8f)); g.fillRect(cx - 18.f, top + 24.f, 14.f, bh1);
        g.setColour(Colour(0xaaffffff)); g.fillRect(cx + 4.f, top + 24.f, 14.f, bh2);
        T(g, "-" + String(sh.gr[b], 1), cx, top + 24.f + mx + 10.f, 1, 1, monoFont(11.f), col);
        T(g, "L -" + String(sh.lgr[b], 1), cx, top + 24.f + mx + 24.f, 1, 1, monoFont(11.f), Colour(0xffe8eaf0));
    }
    T(g, "grey dashed: before   colour: after   left bar: compressor   right bar: limiter (dB)", 8.f, 7.f, 0, 0, monoFont(10.f), Colour(0xff8f93a3));
    if (!sh.active) T(g, "Play audio through Clawpressor to see the bands.", w / 2, h / 2, 1, 1, uiFont(18.f, false, 0.1f), cDim.withAlpha(0.7f));
    for (int i = 1; i < nb; ++i)
    {
        const float x = fxHz(sh.param("x" + String(i)), w), hot = (xDrag == i || xHover == i);
        g.setColour(Colour(hot ? 0xffffffff : 0xccffffff)); g.drawLine(x, top, x, h - 22.f, 1.6f);
        const String t = fmtHz(sh.param("x" + String(i))); const float tw = strW(monoFont(12.f), t) + 12.f;
        const float bx = clampf(x - tw / 2, 2.f, w - tw - 2.f), by = h - 58.f;
        g.setColour(Colour(0xff0b0d12)); g.fillRoundedRectangle(bx, by, tw, 18.f, 4.f); g.setColour(juce::Colours::white); g.drawRoundedRectangle(bx + 0.5f, by + 0.5f, tw - 1, 17.f, 4.f, 1.f);
        T(g, t, bx + tw / 2, by + 9.f, 1, 1, monoFont(12.f), cMint);
        juce::Path tri; tri.addTriangle(x - 6.f, top + 1.f, x + 6.f, top + 1.f, x, top + 9.f); g.setColour(juce::Colours::white); g.fillPath(tri);
    }
}

void DisplayView::drawCurve(Graphics& g, float w, float h)
{
    const int b = sh.selb; const String p = "b" + String(b + 1) + "_"; const Colour col = bandColour(b);
    const float x0 = 56.f, x1 = w - 24.f, y0 = h - 40.f, y1 = 22.f, D0 = -60.f, D1 = 6.f;
    auto X = [&](float d) { return x0 + (d - D0) / (D1 - D0) * (x1 - x0); };
    auto Y = [&](float d) { return y0 - (d - D0) / (D1 - D0) * (y0 - y1); };
    geo = { x0, x1, y0, y1, D0, D1 };
    for (int d = -60; d <= 0; d += 12)
    {
        g.setColour(cGridLine); g.drawLine(X((float) d) + 0.5f, y1, X((float) d) + 0.5f, y0, 1.f); g.drawLine(x0, Y((float) d) + 0.5f, x1, Y((float) d) + 0.5f, 1.f);
        T(g, String(d), X((float) d), y0 + 5.f, 1, 0, monoFont(11.f), cAxisX); T(g, String(d), x0 - 6.f, Y((float) d), 2, 1, monoFont(11.f), cAxisY);
    }
    T(g, "Input level (dB)", (x0 + x1) / 2, y0 + 22.f, 1, 0, monoFont(11.f), cAxisX);
    g.saveState(); g.addTransform(juce::AffineTransform::rotation(-kPi / 2, 14.f, (y0 + y1) / 2)); T(g, "Output level (dB)", 14.f, (y0 + y1) / 2, 1, 1, monoFont(11.f), cAxisY); g.restoreState();
    {
        const float dash[2] = { 5.f, 4.f }; juce::Path d, o; d.startNewSubPath(X(D0), Y(D0)); d.lineTo(X(D1), Y(D1));
        juce::PathStrokeType(1.f).createDashedStroke(o, d, dash, 2); g.setColour(Colour(0xff3a3f4d)); g.fillPath(o);
    }
    const double thr = sh.param(p + "thr"), R = effR(sh.param(p + "ratio"), sh.param("rsteps") > 0.5f), kw = sh.param(p + "soft") > 0.5f ? sh.param(p + "knee") : 0.0;
    const bool on = sh.param(p + "on") > 0.5f, lon = sh.param(p + "lon") > 0.5f; const double fl = sh.param(p + "ufl"), ur = sh.param(p + "uamt"), lt = sh.param(p + "lthr");
    const bool hasUp = on && ur > 1.04;
    if (lon)
    {
        g.setColour(Colour(0x88ffffff)); const float yy = Y((float) lt); for (float xx = x0; xx < x1; xx += 5.f) g.fillRect(xx, yy, 2.f, 1.f);
        T(g, "limiter " + String(lt, 1), x1 - 4.f, Y((float) lt) - 2.f, 2, 2, monoFont(11.f), Colour(0xaaffffff));
    }
    juce::Path cp;
    for (float xx = D0; xx <= D1; xx += 0.5f)
    {
        double o = on ? outAtU(thr, R, kw, xx, fl, ur) : xx; if (lon) o = std::min(o, lt);
        const float px = X(xx), py = Y((float) o); if (xx == D0) cp.startNewSubPath(px, py); else cp.lineTo(px, py);
    }
    g.setColour(col); g.strokePath(cp, juce::PathStrokeType(3.f));
    const float lvl = sh.lvl[b], dot = clampf(lvl, D0, D1);
    if (sh.active && lvl > -80.f)
    {
        double od = on ? outAtU(thr, R, kw, dot, fl, ur) : dot; if (lon) od = std::min(od, lt);
        g.setColour(juce::Colours::white); g.fillEllipse(X(dot) - 6.f, Y((float) od) - 6.f, 12.f, 12.f);
        g.setColour(col); g.drawEllipse(X(dot) - 6.f, Y((float) od) - 6.f, 12.f, 12.f, 2.f);
    }
    handles.clear();
    handles.push_back({ "thr", { X((float) thr), Y((float) thr) } });
    handles.push_back({ "ratio", { X(0.f), Y((float) (on ? outAt(thr, R, 0, 0) : 0.0)) } });
    if (on && sh.param(p + "soft") > 0.5f) { const double kx = thr + kw / 2; handles.push_back({ "knee", { X((float) kx), Y((float) outAt(thr, R, kw, kx)) } }); }
    if (on) { handles.push_back({ "ufl", { X((float) fl), Y((float) fl) } }); handles.push_back({ "uamt", { X(D0), Y((float) outAtU(thr, R, kw, D0, fl, ur)) } }); }
    if (hasUp) { g.setColour(Colour(0x44ffffff)); for (float yy = y1; yy < y0; yy += 7.f) g.fillRect(X((float) fl), yy, 1.f, 3.f); }
    for (auto& hd : handles)
    {
        g.setColour(cDrag == hd.first ? juce::Colours::white : (hd.first == "thr" || hd.first == "ratio") ? Colour(0xff0b0d12) : Colour(0xff1d2230));
        g.fillEllipse(hd.second.x - 8.f, hd.second.y - 8.f, 16.f, 16.f);
        g.setColour(col); g.drawEllipse(hd.second.x - 8.f, hd.second.y - 8.f, 16.f, 16.f, 2.5f);
    }
    T(g, String(bandName(b)).toUpperCase() + " BAND", x0 + 10.f, y1 + 4.f, 0, 0, uiFont(13.f, true, 0.08f), Colour(0xffe8eaf0));
    curveInfo = "Dots: threshold, ratio, knee (on the bend), lift floor and lift slope (lower left). Now: threshold (" + String(thr, 1) + " dB), ratio ("
              + (R > 1e8 ? String("inf") : String(R, 1)) + ":1). White dot: the signal right now.";
}

void DisplayView::drawScope(Graphics& g, float w, float h)
{
    const int b = sh.selb; const Colour col = bandColour(b);
    g.setColour(cGridLine); for (int i = 1; i < 8; ++i) g.drawLine(0.f, h * (float) i / 8.f + 0.5f, w, h * (float) i / 8.f + 0.5f, 1.f);
    g.setColour(Colour(0xff2c3a38)); g.drawLine(0.f, h / 2 + 0.5f, w, h / 2 + 0.5f, 1.f);
    auto& e = sh.proc.engine; const int N = claw::Engine::kScope, idx = e.scopeIdx.load(std::memory_order_acquire);
    const double sr = sh.proc.getSampleRate() > 0 ? sh.proc.getSampleRate() : 44100.0;
    const int win = std::min(4000, (int) std::round(sr * 0.05));
    std::vector<float> A((size_t) N), B((size_t) N);
    for (int j = 0; j < N; ++j) { A[(size_t) j] = e.scopeA[(size_t) b][(size_t) ((idx + j) & (N - 1))]; B[(size_t) j] = e.scopeB[(size_t) b][(size_t) ((idx + j) & (N - 1))]; }
    int st = N - win - 2048;
    for (int j = N - win - 4096; j < N - win; ++j) if (A[(size_t) j] <= 0.f && A[(size_t) j + 1] > 0.f) { st = j; break; }
    const float scale = h / 2 * 0.9f; bool any = false;
    for (int j = 0; j < win; ++j) if (std::fabs(A[(size_t) (st + j)]) > 1e-4f) { any = true; break; }
    const int cols = (int) std::floor(w); const float per = (float) win / (float) cols;
    for (int x = 0; x < cols; ++x)
    {
        float a0 = 1, a1 = -1, b0 = 1, b1 = -1; const int k0 = (int) std::floor((float) st + (float) x * per), k1 = std::max(k0 + 1, (int) std::floor((float) st + (float) (x + 1) * per));
        for (int j = k0; j < k1 && j < N; ++j) { const float va = A[(size_t) j], vb = B[(size_t) j]; a0 = std::min(a0, va); a1 = std::max(a1, va); b0 = std::min(b0, vb); b1 = std::max(b1, vb); }
        if (a0 > a1) continue;
        const float yA0 = h / 2 - a1 * scale, yA1 = h / 2 - a0 * scale, yB0 = h / 2 - b1 * scale, yB1 = h / 2 - b0 * scale;
        g.setColour(Colour(0xffff3b5c)); g.fillRect((float) x, yA0, 1.f, std::max(1.f, yA1 - yA0));
        g.setColour(Colour(0xff3cf08a)); g.fillRect((float) x, yB0, 1.f, std::max(1.f, yB1 - yB0));
        const float lo = std::max(yA0, yB0), hi = std::min(yA1, yB1);
        if (hi > lo) { g.setColour(Colour(0xffb8bcc8)); g.fillRect((float) x, lo, 1.f, hi - lo); }
    }
    T(g, String(bandName(b)).toUpperCase() + " BAND  " + String(juce::roundToInt((double) win / sr * 1000.0)) + " ms", 10.f, 8.f, 0, 0, uiFont(13.f, true, 0.08f), col);
    T(g, "original", 10.f, 28.f, 0, 0, monoFont(11.f), Colour(0xffff6b86)); T(g, "after", 80.f, 28.f, 0, 0, monoFont(11.f), Colour(0xff6bf5a3)); T(g, "overlap", 130.f, 28.f, 0, 0, monoFont(11.f), Colour(0xffb8bcc8));
    if (!any) T(g, "No signal in this band. Play audio through the plugin.", w / 2, h / 2 - 20.f, 1, 0, monoFont(11.f), Colour(0xff8f93a3));
}

void DisplayView::drawGR(Graphics& g, float w, float h)
{
    const float top = 20.f, bot = h - 26.f, span = sh.grSpan, maxDb = 24.f; const int nb = sh.nb();
    auto Yd = [&](float d) { return top + clampf(d, 0.f, maxDb) / maxDb * (bot - top); };
    for (int d = 0; d <= 24; d += 6)
    {
        g.setColour(cGridLine); g.drawLine(34.f, Yd((float) d) + 0.5f, w, Yd((float) d) + 0.5f, 1.f);
        T(g, d == 0 ? String("0") : "-" + String(d), 30.f, Yd((float) d), 2, 1, monoFont(11.f), cAxisY);
    }
    for (int i = 0; i <= 5; ++i)
    {
        const float t = span * (float) i / 5.f, x = 34.f + (w - 34.f) * (1.f - t / span);
        g.setColour(Colour(0xff161f1e)); g.drawLine(x + 0.5f, top, x + 0.5f, bot, 1.f);
        T(g, i == 0 ? String("now") : "-" + String(t, std::fmod(t, 1.f) > 0.01f ? 1 : 0) + " s", i == 0 ? x - 2.f : x, bot + 6.f, i == 0 ? 2 : 1, 0, monoFont(11.f), cAxisX);
    }
    const Colour cols[5] = { cMint, cYellow, cPink, cPurple, juce::Colours::white };
    const int n = (int) std::round(span * sh.histFps);
    for (int k = 4; k >= 0; --k)
    {
        if (k < 4 && k >= nb) continue; auto& H = sh.hist[k]; if (H.empty()) continue;
        juce::Path p; bool first = true;
        for (int i = 0; i < n; ++i)
        {
            const int ix = (int) H.size() - 1 - i; if (ix < 0) break;
            const float xx = 34.f + (w - 34.f) * (1.f - (float) i / (float) n), yy = Yd(H[(size_t) ix]);
            if (first) { p.startNewSubPath(xx, yy); first = false; } else p.lineTo(xx, yy);
        }
        g.setColour(cols[k]); g.strokePath(p, juce::PathStrokeType(k == 4 ? 2.2f : 1.6f));
    }
    float lx = 44.f;
    for (int k = 0; k < 5; ++k)
    {
        if (k < 4 && k >= nb) continue;
        g.setColour(cols[k]); g.fillRect(lx, 5.f, 10.f, 10.f);
        const String nm = k == 4 ? String("MASTER LIMITER") : String(bandName(k)).toUpperCase(); const auto f = uiFont(12.f, true, 0.08f);
        T(g, nm, lx + 14.f, 3.f, 0, 0, f, Colour(0xffe8eaf0)); lx += 24.f + strW(f, nm);
    }
}

void DisplayView::drawPat(Graphics& g, float w, float h)
{
    const float ml = 96.f, mt = 6.f, mb = 22.f, cw = (w - ml - 12.f) / 16.f, gap = 8.f, lh = (h - mt - mb - 2 * gap) / 3.f;
    pgeo = { ml, mt, cw, lh, gap, w, h };
    const bool lanesOn[3] = { sh.param("pon") > 0.5f, sh.param("la_on") > 0.5f, sh.param("lb_on") > 0.5f };
    const int cur0 = sh.active ? sh.step : -1; const String keys[3] = { "pat", "la_s", "lb_s" };
    for (int ln = 0; ln < 3; ++ln)
    {
        const float top = mt + (float) ln * (lh + gap); const bool on = lanesOn[ln];
        g.setColour(Colour(0xff0b1110)); g.fillRect(ml, top, w - ml - 12.f, lh);
        T(g, "LANE " + String(ln + 1), ml - 6.f, top + 2.f, 2, 0, uiFont(12.f, true, 0.08f), on ? Colour(0xffe8eaf0) : Colour(0xff6a6d7a));
        String tn = "gain dip";
        if (ln > 0)
        {
            static const char* kn[4] = { "input", "output", "sat", "width" };
            const int tg = (int) sh.param(ln == 1 ? "la_tgt" : "lb_tgt");
            tn = tg < 3 ? String(tg == 0 ? "Mix" : tg == 1 ? "Master out" : "Post gain") : "B" + String(1 + ((tg - 3) >> 2)) + " " + kn[(tg - 3) & 3];
        }
        T(g, tn, ml - 6.f, top + 17.f, 2, 0, monoFont(10.f), cAxisY);
        for (int i = 0; i < 16; ++i)
        {
            const float x = ml + (float) i * cw; const int lev = (int) sh.param(keys[ln] + String(i)); const bool cur = on && cur0 == i;
            g.setColour(i % 4 == 0 ? Colour(0xff0f1816) : Colour(0xff0b1110)); g.fillRect(x + 2.f, top, cw - 4.f, lh);
            if (ln == 0)
            {
                if (lev > 0) { const Colour cc[4] = { cMint, Colour(0xff398BC4), cPurple, cPink }; g.setColour(cc[lev]); g.fillRect(x + 4.f, top, cw - 8.f, (float) lev * lh / 4.f); g.setColour(Colour(0x55ffffff)); g.fillRect(x + 4.f, top + (float) lev * lh / 4.f - 3.f, cw - 8.f, 3.f); }
                else { g.setColour(Colour(0x3302EBBA)); g.fillRect(x + 4.f, top, cw - 8.f, 4.f); }
            }
            else
            {
                const float bh = (float) lev / 4.f * (lh - 4.f);
                if (lev > 0) { const Colour cc[5] = { cMint, Colour(0xff398BC4), cMint, cYellow, cPink }; g.setColour(cc[lev]); g.fillRect(x + 4.f, top + lh - bh, cw - 8.f, bh); g.setColour(Colour(0x55ffffff)); g.fillRect(x + 4.f, top + lh - bh, cw - 8.f, 3.f); }
                else { g.setColour(Colour(0x3302EBBA)); g.fillRect(x + 4.f, top + lh - 4.f, cw - 8.f, 4.f); }
            }
            if (cur) { g.setColour(cYellow); g.drawRect(x + 2.f, top + 1.f, cw - 4.f, lh - 2.f, 2.f); }
            if (!on) { g.setColour(Colour(0xcc06080a)); g.fillRect(x + 2.f, top, cw - 4.f, lh); }
        }
    }
    for (int i = 0; i < 16; ++i) T(g, String(i + 1), ml + (float) i * cw + cw / 2, h - mb + 6.f, 1, 0, monoFont(11.f), (sh.active && sh.step == i && lanesOn[0]) ? cYellow : cAxisX);
    String now = "off";
    if (lanesOn[0] && sh.step >= 0)
    {
        static const float PD[4] = { 0, 4, 9, 16 }; const float db = PD[juce::jlimit(0, 3, (int) sh.param("pat" + String(sh.step)))] * sh.param("pstr") * 0.01f * (sh.param("px2") > 0.5f ? 2.f : 1.f);
        now = "now -" + String(db, 1) + " dB";
    }
    T(g, now, ml - 6.f, mt + 31.f, 2, 0, monoFont(10.f), Colour(0xff8f93a3));
}

// ------------------------------------------------------------------ display mouse
void DisplayView::touch(const String& id, float v)
{
    if (touched.insert(id).second) sh.gesture(id, true);
    sh.setParam(id, v);
}
void DisplayView::endTouches() { for (auto& id : touched) sh.gesture(id, false); touched.clear(); }
int DisplayView::hitX(float x) const
{
    int best = -1; float bd = 12.f; const float w = (float) getWidth();
    for (int i = 1; i < sh.nb(); ++i) { const float d = std::fabs(x - fxHz(sh.param("x" + String(i)), w)); if (d < bd) { bd = d; best = i; } }
    return best;
}
void DisplayView::curvePtr(juce::Point<float> pt, bool down)
{
    const String p = "b" + String(sh.selb + 1) + "_";
    if (down)
    {
        float bd = 22.f; String best;
        for (auto& hd : handles) { const float d = pt.getDistanceFrom(hd.second); if (d < bd) { bd = d; best = hd.first; } }
        cDrag = best;
    }
    if (cDrag.isEmpty()) return;
    const auto& G = geo; const float xd = G.D0 + (pt.x - G.x0) / (G.x1 - G.x0) * (G.D1 - G.D0), yd = G.D0 + (G.y0 - pt.y) / (G.y0 - G.y1) * (G.D1 - G.D0);
    const float thr = sh.param(p + "thr");
    if (cDrag == "thr") touch(p + "thr", std::round(clampf(xd, -60.f, 0.f) * 2.f) / 2.f);
    else if (cDrag == "ratio") { const float R = (0.f - thr) / std::max(0.01f, yd - thr); touch(p + "ratio", std::round(clampf(R, 1.f, 20.f) * 10.f) / 10.f); }
    else if (cDrag == "knee") touch(p + "knee", std::round(clampf(2.f * (xd - thr), 0.f, 24.f) * 2.f) / 2.f);
    else if (cDrag == "ufl") touch(p + "ufl", std::round(clampf(xd, -60.f, std::min(-20.f, thr - 6.f)) * 2.f) / 2.f);
    else if (cDrag == "uamt")
    {
        float fl = sh.param(p + "ufl"); float ur = (fl - G.D0) / std::max(0.5f, fl - yd);
        if (fl - G.D0 < 3.f) { touch(p + "ufl", std::min(-20.f, G.D0 + 12.f)); fl = sh.param(p + "ufl"); ur = (fl - G.D0) / std::max(0.5f, fl - yd); }
        touch(p + "uamt", std::round(clampf(ur, 1.f, 4.f) * 10.f) / 10.f);
    }
}
void DisplayView::patPtr(juce::Point<float> pt, bool down)
{
    const auto& G = pgeo; if (G.cw <= 0.f) return;
    if (down)
    {
        const int ln = (int) std::floor((pt.y - G.mt) / (G.lh + G.gap)); if (ln < 0 || ln > 2) return;
        patLane = ln; patPaint = true;
        if (ln > 0 && sh.param(ln == 1 ? "la_on" : "lb_on") < 0.5f) touch(ln == 1 ? "la_on" : "lb_on", 1.f);
        if (ln == 0 && sh.param("pon") < 0.5f) touch("pon", 1.f);
    }
    if (!patPaint) return;
    const int i = (int) std::floor((pt.x - G.ml) / G.cw); if (i < 0 || i > 15) return;
    const float top = G.mt + (float) patLane * (G.lh + G.gap);
    if (patLane == 0) { int lev = (int) clampf(std::ceil((pt.y - top) / (G.lh / 4.f)), 0.f, 3.f); if (pt.y < top + 4.f) lev = 0; touch("pat" + String(i), (float) lev); }
    else touch(String(patLane == 1 ? "la" : "lb") + "_s" + String(i), clampf(std::round((top + G.lh - pt.y) / (G.lh - 4.f) * 4.f), 0.f, 4.f));
}
void DisplayView::mouseDown(const juce::MouseEvent& e)
{
    if (sh.view == 0) { const int i = hitX(e.position.x); if (i > 0) { xDrag = i; touch("x" + String(i), sh.param("x" + String(i))); } }
    else if (sh.view == 1) curvePtr(e.position, true);
    else if (sh.view == 4) patPtr(e.position, true);
}
void DisplayView::mouseDrag(const juce::MouseEvent& e)
{
    if (sh.view == 0 && xDrag > 0)
    {
        const float w = (float) getWidth(), f = xHz(clampf(e.position.x, 0.f, w), w);
        const float lo = xDrag > 1 ? sh.param("x" + String(xDrag - 1)) * 1.25f : 30.f, hi = xDrag < sh.nb() - 1 ? sh.param("x" + String(xDrag + 1)) / 1.25f : 16000.f;
        touch("x" + String(xDrag), std::round(clampf(f, lo, std::max(lo, hi))));
    }
    else if (sh.view == 1) curvePtr(e.position, false);
    else if (sh.view == 4) patPtr(e.position, false);
}
void DisplayView::mouseUp(const juce::MouseEvent&) { endTouches(); xDrag = -1; cDrag = {}; patPaint = false; }
void DisplayView::mouseMove(const juce::MouseEvent& e)
{
    if (sh.view != 0) return;
    xHover = hitX(e.position.x); setMouseCursor(xHover > 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}
void DisplayView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (sh.view != 0) return;
    const int i = hitX(e.position.x);
    if (i > 0) if (auto* p = sh.proc.apvts.getParameter("x" + String(i))) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
}
void DisplayView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    if (sh.view != 0) return;
    const int i = hitX(e.position.x); if (i < 1) return;
    const float lo = i > 1 ? sh.param("x" + String(i - 1)) * 1.25f : 30.f, hi = i < sh.nb() - 1 ? sh.param("x" + String(i + 1)) / 1.25f : 16000.f;
    const String id = "x" + String(i);
    sh.gesture(id, true); sh.setParam(id, std::round(clampf(sh.param(id) * (d.deltaY > 0 ? 1.03f : 0.97f), lo, std::max(lo, hi)))); sh.gesture(id, false);
}

// ------------------------------------------------------------------ level panel
LevelPanel::LevelPanel(Shared& s) : sh(s)
{
    modeBtn.getProperties().set("fs", 11.0);
    modeBtn.getProperties().set("tip", "Density\nSwitch the level meter between peak levels and the density view.");
    modeBtn.onClick = [this] { sh.density = !sh.density; modeBtn.setButtonText(sh.density ? "Level" : "Density"); repaint(); };
    addAndMakeVisible(modeBtn);
}
void LevelPanel::resized()
{
    modeBtn.setBounds(getWidth() - 8 - 72, 5, 72, 22);
    cv = Rectangle<int>(8, 34, getWidth() - 16, getHeight() - 34 - 6 - 45 - 6 - 12 - 8);
}
static void drawMeters(Graphics& g, Shared& sh, float w, float h)
{
    const float top = 14.f, bot = h - 20.f;
    auto Yd = [&](float d) { return top + (3.f - clampf(d, -54.f, 3.f)) / 57.f * (bot - top); };
    g.setColour(cDisp); g.fillRect(0.f, 0.f, w, h);
    const float sx = w / 2 - 14.f;
    for (int d = 0; d >= -54; d -= 6)
    {
        const float y = std::round(Yd((float) d)) + 0.5f;
        g.setColour(cGridLine); g.drawLine(0.f, y, w, y, 1.f);
        T(g, d == 0 ? String("0") : String(d), sx + 14.f, y, 1, 1, monoFont(10.f), d >= -6 ? Colour(0xffffb35c) : cAxisY);
    }
    auto bar = [&](float x, float bw, float val, float hold, Colour col)
    {
        const float y = Yd(val);
        juce::ColourGradient gr(cPink, 0.f, top, col, 0.f, bot, false); gr.addColour(0.1, cYellow); gr.addColour(0.35, col);
        g.setGradientFill(gr); g.fillRect(x, y, bw, bot - y);
        g.setColour(juce::Colours::white); g.fillRect(x, Yd(hold), bw, 2.f);
    };
    const Colour grey(0xff8f93a3);
    if (!sh.density)
    {
        const float bw = std::max(8.f, (sx - 14.f) / 4.f);
        bar(4.f, bw, sh.inL, sh.hInL, cMint); bar(4.f + bw + 3.f, bw, sh.inR, sh.hInR, cMint);
        T(g, "IN", 4.f + bw + 1.f, bot + 4.f, 1, 0, monoFont(10.f), grey);
        const float ox = sx + 36.f;
        bar(ox, bw, sh.outL, sh.hOutL, Colour(0xff6CC9F5)); bar(ox + bw + 3.f, bw, sh.outR, sh.hOutR, Colour(0xff6CC9F5));
        T(g, "OUT", ox + bw + 1.f, bot + 4.f, 1, 0, monoFont(10.f), grey);
        const float yo = std::round(Yd(-6.f)) + 0.5f;
        g.setColour(Colour(0xffffb35c)); for (float xx = 0.f; xx < sx - 4.f; xx += 6.f) g.fillRect(xx, yo, 3.f, 1.f);
        T(g, "opt -6", 3.f, yo - 8.f, 0, 1, monoFont(10.f), Colour(0xffffb35c));
        g.setColour(sh.clipT > 0.f ? Colour(0xffff2d95) : Colour(0xff3a1020)); g.fillRect(ox, 2.f, 2.f * bw + 3.f, 6.f);
    }
    else
    {
        const float bw2 = std::max(10.f, (w - 56.f) / 3.f), x2 = 6.f, pk = std::max(sh.hOutL, sh.hOutR);
        bar(x2, bw2, pk, pk, Colour(0xff6CC9F5)); bar(x2 + bw2 + 6.f, bw2, sh.rms, sh.rms, cMint);
        T(g, "PEAK", x2 + bw2 / 2, bot + 4.f, 1, 0, monoFont(10.f), grey); T(g, "RMS", x2 + bw2 * 1.5f + 6.f, bot + 4.f, 1, 0, monoFont(10.f), grey);
        const float yp = Yd(pk), yr = Yd(sh.rms);
        g.setColour(Colour(0x55FF1383)); g.fillRect(x2 + bw2 * 2 + 12.f, yp, 18.f, std::max(0.f, yr - yp));
        T(g, String(sh.crest, 1), x2 + bw2 * 2 + 12.f, (yp + yr) / 2 - 8.f, 0, 1, monoFont(10.f), Colour(0xffe8eaf0));
        T(g, "crest", x2 + bw2 * 2 + 12.f, (yp + yr) / 2 + 4.f, 0, 1, monoFont(10.f), Colour(0xffe8eaf0));
    }
}
void LevelPanel::paint(Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.setColour(cPanel2); g.fillRoundedRectangle(0, 0, w, h, 4.f); g.setColour(cLine); g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 4.f, 1.f);
    T(g, sh.density ? "DENSITY" : "LEVEL", 10.f, 16.f, 0, 1, uiFont(12.f, false, 0.2f), cDim);
    g.saveState(); g.reduceClipRegion(cv); g.setOrigin(cv.getPosition()); drawMeters(g, sh, (float) cv.getWidth(), (float) cv.getHeight()); g.restoreState();
    g.setColour(cLine); g.drawRect(cv.expanded(1).toFloat(), 1.f);
    auto f = [](float v) { return v > -89.f ? String(v, 1) : String("-inf"); };
    const String lab[6] = { "In peak", "Out peak", "RMS", "Crest", "Master GR", "Mix" };
    const String val[6] = { f(std::max(sh.hInL, sh.hInR)), f(std::max(sh.hOutL, sh.hOutR)), f(sh.rms), String(sh.crest, 1), "-" + String(sh.mgr, 1), fmtPct(sh.param("mix")) };
    for (int i = 0; i < 6; ++i)
    {
        const float x0 = i % 2 ? w / 2 + 5.f : 8.f, x1 = i % 2 ? w - 8.f : w / 2 - 5.f, y = (float) cv.getBottom() + 12.f + (float) (i / 2) * 15.f;
        T(g, lab[i], x0, y, 0, 1, monoFont(11.f), cDim); T(g, val[i], x1, y, 2, 1, monoFont(11.f), cYellow);
    }
    const float dy = h - 8.f - 12.f; g.setColour(cDisp); g.fillRoundedRectangle(8.f, dy, w - 16.f, 12.f, 3.f);
    juce::ColourGradient gr(Colour(0xff398BC4), 8.f, 0.f, cPink, w - 8.f, 0.f, false); gr.addColour(0.35, cMint); gr.addColour(0.7, cYellow);
    g.setGradientFill(gr); g.fillRoundedRectangle(8.f, dy, (w - 16.f) * sh.dens, 12.f, 3.f);
    g.setColour(cLine); g.drawRoundedRectangle(8.5f, dy + 0.5f, w - 17.f, 11.f, 3.f, 1.f);
}

// ------------------------------------------------------------------ card (guide, settings, about, save)
Card::Card() { setWantsKeyboardFocus(true); }
void Card::setContent(const String& t, std::vector<Block> b)
{
    title = t; blocks = std::move(b);
    juce::AttributedString as; as.setJustification(juce::Justification::topLeft);
    bool first = true;
    for (auto& k : blocks)
    {
        if (k.kind == Block::H) { if (!first) as.append("\n", uiFont(8.f), cInk); as.append(k.text.toUpperCase() + "\n", uiFont(13.f, true, 0.2f), cMint); }
        else as.append((k.kind == Block::Li ? juce::String(juce::CharPointer_UTF8("\xe2\x80\xa2  ")) : String()) + k.text + "\n", uiFont(15.f, false, 0.02f), Colour(0xffd6d8de));
        first = false;
    }
    layout.createLayout(as, 520.f);
    resized(); repaint();
}
void Card::addButton(const String& text, std::function<void()> fn, bool primary)
{
    auto b = std::make_unique<juce::TextButton>(text); b->setToggleState(primary, juce::dontSendNotification); b->onClick = std::move(fn);
    addAndMakeVisible(*b); buttons.push_back(std::move(b)); resized();
}
void Card::addInput(const String& initial)
{
    input = std::make_unique<juce::TextEditor>(); input->setFont(monoFont(14.f)); input->setText(initial); input->setSelectAllWhenFocused(true);
    input->setInputRestrictions(40); addAndMakeVisible(*input); resized(); input->grabKeyboardFocus();
}
void Card::addToggleRow(const String& text, bool initial, std::function<void(bool)> fn)
{
    auto r = std::make_unique<TRow>(); r->text = text; r->b = std::make_unique<juce::TextButton>(initial ? "On" : "Off");
    auto* bp = r->b.get(); bp->setToggleState(initial, juce::dontSendNotification);
    bp->onClick = [bp, fn] { const bool v = !bp->getToggleState(); bp->setToggleState(v, juce::dontSendNotification); bp->setButtonText(v ? "On" : "Off"); fn(v); };
    addAndMakeVisible(*bp); rows.push_back(std::move(r)); resized();
}
void Card::resized()
{
    const int W = 560, lh = (int) std::ceil(layout.getHeight());
    const int H = 56 + lh + (int) rows.size() * 40 + (input ? 44 : 0) + 64;
    box = Rectangle<int>(W, H).withCentre(getLocalBounds().getCentre());
    int y = box.getY() + 56 + lh;
    for (auto& r : rows) { r->b->setBounds(box.getRight() - 18 - 70, y + 2, 70, 28); y += 40; }
    if (input) { input->setBounds(box.getX() + 18, y + 6, W - 36, 30); y += 44; }
    int x = box.getRight() - 18;
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it)
    {
        const int bw = juce::jmax(80, (int) strW(uiFont(13.f, true), (*it)->getButtonText()) + 30);
        (*it)->setBounds(x - bw, box.getBottom() - 18 - 32, bw, 32); x -= bw + 8;
    }
}
void Card::paint(Graphics& g)
{
    g.fillAll(Colour(0xaa000000));
    g.setColour(cPanel); g.fillRoundedRectangle(box.toFloat(), 6.f); g.setColour(Colour(0xff4a4d58)); g.drawRoundedRectangle(box.toFloat().reduced(0.5f), 6.f, 1.f);
    T(g, title.toUpperCase(), (float) box.getX() + 18.f, (float) box.getY() + 30.f, 0, 1, uiFont(22.f, true, 0.14f), cInk);
    layout.draw(g, Rectangle<float>((float) box.getX() + 18.f, (float) box.getY() + 54.f, 524.f, layout.getHeight() + 4.f));
    int y = box.getY() + 56 + (int) std::ceil(layout.getHeight());
    for (auto& r : rows) { T(g, r->text, (float) box.getX() + 18.f, (float) y + 16.f, 0, 1, uiFont(15.f, false, 0.02f), Colour(0xffd6d8de)); y += 40; }
}
void Card::mouseDown(const juce::MouseEvent& e) { if (!box.contains(e.getPosition()) && onClose) onClose(); }
bool Card::keyPressed(const juce::KeyPress& k) { if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; } return false; }

} // namespace clawui

// ------------------------------------------------------------------ editor
namespace
{
const int kCtlH[5] = { 58, 84, 34, 58, 172 };
const char* kTabNames[5] = { "Bands", "Curve", "Band scope", "GR history", "Pattern" };
const char* kTabTips[5] = {
    "Spectrum before (grey line) and after (colour fill) with draggable crossover lines and the gain reduction of every band.",
    "Compressor curve of the selected band: input on the bottom, output up the side. Drag the corner to set the threshold and the end to set the ratio.",
    "Waveform of the selected band: red is the original, green is after compression, grey is where they overlap.",
    "Gain reduction history of every band and the master limiter.",
    "Pattern gain: draw a rhythmic volume pattern over 16 steps, plus two lanes that move any parameter." };
struct NamedPat { const char* name; int v[16]; };
const NamedPat kPats[] = {
    { "Draw your own", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }, { "Pump on the beat", { 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0 } },
    { "Offbeat dip", { 0, 0, 3, 1, 0, 0, 3, 1, 0, 0, 3, 1, 0, 0, 3, 1 } }, { "Roll 16ths", { 0, 2, 0, 3, 0, 2, 0, 3, 0, 2, 0, 3, 0, 2, 0, 3 } },
    { "Half-bar breath", { 3, 3, 2, 2, 1, 1, 0, 0, 3, 3, 2, 2, 1, 1, 0, 0 } } };
const NamedPat kLanePats[] = {
    { "Off", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }, { "Offbeat", { 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0 } },
    { "Ramp up", { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4 } }, { "Ramp down", { 4, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0 } },
    { "Gate 1/8", { 4, 4, 0, 0, 4, 4, 0, 0, 4, 4, 0, 0, 4, 4, 0, 0 } }, { "3-3-2", { 4, 0, 0, 3, 0, 0, 4, 0, 4, 0, 0, 3, 0, 0, 4, 0 } },
    { "Rolling 16ths", { 4, 2, 3, 2, 4, 2, 3, 2, 4, 2, 3, 2, 4, 2, 3, 2 } } };

void applyTipsRec(juce::Component& c, bool on)
{
    if (c.getProperties().contains("tip"))
        if (auto* t = dynamic_cast<juce::SettableTooltipClient*>(&c)) t->setTooltip(on ? c.getProperties()["tip"].toString() : String());
    for (auto* ch : c.getChildren()) applyTipsRec(*ch, on);
}
void setProp(juce::Component& c, const String& title, const String& text) { c.getProperties().set("tip", title + "\n" + text); }
}

ClawpressorEditor::ClawpressorEditor(ClawpressorProcessor& p)
    : AudioProcessorEditor(&p), proc(p), sh(p), display(sh), level(sh)
{
    setLookAndFeel(&look);
    addAndMakeVisible(content);
    auto& st = proc.apvts.state;
    sh.view = juce::jlimit(0, 4, (int) st.getProperty("view", 0)); sh.selb = juce::jlimit(0, 3, (int) st.getProperty("selb", 0));
    sh.grSpan = (float) (double) st.getProperty("grspan", 10.0);
#ifdef CLAW_SHOT
    if (auto* v = std::getenv("CLAW_VIEW")) sh.view = std::atoi(v);
    if (auto* v = std::getenv("CLAW_BAND")) sh.selb = std::atoi(v);
#endif
    fps = (bool) st.getProperty("eco", false) ? 12 : 30; sh.histFps = (float) fps;
    {
        const char* svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\"><path d=\"M4.5 5C10 7 17 11 21 14c3-.5 7-2 11-2s8 1.5 11 2C47 11 55 7 60 5.5L61 24C61 40 48 50 32 58 16 50 3 40 3.7 24z\" fill=\"#f4f5f7\"/><path d=\"M11.3 25.8C20 26 25 31 27 36.5 21 37.5 15 36 12.5 31zM52.7 25.8C44 26 39 31 37 36.5 43 37.5 49 36 51.5 31z\" fill=\"#15161a\"/></svg>";
        if (auto xml = juce::parseXML(svg)) logo = juce::Drawable::createFromSVG(*xml);
    }
    content.addAndMakeVisible(display); content.addAndMakeVisible(level);
    setProp(display, "Display", "Drag the white crossover lines in the Bands view. In Curve, drag the dots. In Pattern, click or drag the steps.");
    for (int b = 0; b < 4; ++b) { strips[(size_t) b] = std::make_unique<BandStrip>(sh, b); content.addAndMakeVisible(*strips[(size_t) b]); }
    for (int i = 0; i < 5; ++i)
    {
        tabs[(size_t) i].setButtonText(kTabNames[i]); tabs[(size_t) i].getProperties().set("fs", 15.0);
        tabs[(size_t) i].onClick = [this, i] { setView(i); };
        setProp(tabs[(size_t) i], kTabNames[i], kTabTips[i]); content.addAndMakeVisible(tabs[(size_t) i]);
    }
    for (int i = 0; i < 4; ++i)
    {
        bsel[(size_t) i].setButtonText(String(i + 1)); bsel[(size_t) i].setComponentID("bsel" + String(i + 1)); bsel[(size_t) i].getProperties().set("fs", 13.0);
        bsel[(size_t) i].onClick = [this, i] { selectBand(i); };
        setProp(bsel[(size_t) i], "Band " + String(i + 1), "Select band " + String(i + 1) + " for the Curve and Band scope views.");
        content.addAndMakeVisible(bsel[(size_t) i]);
    }
    pName.setComponentID("pname"); pName.onClick = [this] { showPresetMenu(); };
    pPrev.onClick = [this] { stepPreset(-1); }; pNext.onClick = [this] { stepPreset(1); };
    pSave.onClick = [this] { showSaveCard(); }; pMore.onClick = [this] { showMoreMenu(); };
    bGuide.onClick = [this] { showGuide(); }; bSet.onClick = [this] { showSettings(); }; bAbout.onClick = [this] { showAbout(); };
    setProp(pPrev, "Previous preset", "Previous preset."); setProp(pNext, "Next preset", "Next preset.");
    setProp(pName, "Presets", "Open the preset list: factory presets and your own.");
    setProp(pSave, "Save", "Save the current settings as your own preset (stored in Studio 87/Clawpressor/presets).");
    setProp(pMore, "More", "Delete, export or import your presets.");
    setProp(bGuide, "Guide", "Quick guide: what each view and control does.");
    setProp(bSet, "Settings", "Info bubbles and eco mode.");
    setProp(bAbout, "About & Support", "About Studio 87, privacy, and ways to support the project.");
    for (auto* b : { &pPrev, &pName, &pNext, &pSave, &pMore, &bGuide, &bSet, &bAbout }) { content.addAndMakeVisible(*b); b->getProperties().set("fs", 13.0); }
    pName.setButtonText(presetLabel());

    kMix = std::make_unique<Knob>(sh, "mix", "Mix", fmtPct, cMint, kNoMid, "Dry/wet of the whole plugin. 100 % is wet only. Around 75 % brings back dynamic peaks without losing much density (parallel compression). Default 50 %.");
    kOut = std::make_unique<Knob>(sh, "out", "Output", fmtDb, cMint, 0.f, "Level into the master limiter.");
    kPost = std::make_unique<Knob>(sh, "post", "Post gain", fmtDb, cMint, 0.f, "Level after the master limiter.");
    kMrel = std::make_unique<Knob>(sh, "mrel", "Mstr rel", fmtMs, cMint, kNoMid, "Release of the master output limiter.");
    for (auto* k : { kMix.get(), kOut.get(), kPost.get(), kMrel.get() }) content.addAndMakeVisible(*k);
    tMlim = std::make_unique<Tog>(sh, "mlim", "Master limiter", "Safety limiter at the very end, after the mix, so peaks of the dry part are caught too. Ceiling -0.3 dB. Looks ahead 64 samples (reported to the host as latency).");
    tAutog = std::make_unique<Tog>(sh, "autog", "Autogain", "Compressors add makeup gain automatically so loudness stays about the same when you compress harder.");
    content.addAndMakeVisible(*tMlim); content.addAndMakeVisible(*tAutog);

    sNb = std::make_unique<Sel>(sh, "nb", "Bands", "How many bands the signal is split into. Drag the white crossover lines in the display to move them.");
    sSlope = std::make_unique<Sel>(sh, "slope", "Crossover slope", "Steepness of the crossover filters. Steeper keeps the bands cleaner.");
    tRsteps = std::make_unique<Tog>(sh, "rsteps", "Whole ratios", "Snaps every ratio to whole numbers (2:1, 3:1, 4:1 ...).");
    for (juce::Component* c : { (juce::Component*) sNb.get(), (juce::Component*) sSlope.get(), (juce::Component*) tRsteps.get() }) content.addAndMakeVisible(*c);
    auto hint = [&](juce::Label& l, const String& t) { l.setText(t, juce::dontSendNotification); l.setFont(uiFont(12.5f, false, 0.04f)); l.setColour(juce::Label::textColourId, cDim);
                                                       l.setJustificationType(juce::Justification::centredRight); l.setMinimumHorizontalScale(1.f); l.setInterceptsMouseClicks(false, false); content.addAndMakeVisible(l); };
    hint(hintBands, "Drag a white line to move a crossover. Double-click for default.");
    hint(hintCurve, ""); hint(hintScope, "Red: original. Green: after. Grey: overlap. Pick the band top right.");
    hint(hintGr, "Band colours and the master limiter (white). Down is more gain reduction.");
    hint(hintPat, "Click or drag the steps. Lane 1 dips the gain. Lanes 2 and 3 move any parameter.");

    cSpan.addItem("5 s", 1); cSpan.addItem("10 s", 2); cSpan.addItem("20 s", 3); cSpan.addItem("30 s", 4);
    cSpan.setSelectedId(sh.grSpan <= 5.f ? 1 : sh.grSpan <= 10.f ? 2 : sh.grSpan <= 20.f ? 3 : 4, juce::dontSendNotification);
    cSpan.onChange = [this] { static const float v[4] = { 5, 10, 20, 30 }; sh.grSpan = v[juce::jlimit(0, 3, cSpan.getSelectedId() - 1)]; proc.apvts.state.setProperty("grspan", (double) sh.grSpan, nullptr); };
    setProp(cSpan, "History", "How much time the gain reduction history shows.");
    content.addAndMakeVisible(cSpan);

    tPon = std::make_unique<Tog>(sh, "pon", "Pattern on", "Turns the pattern gain on or off.");
    sPspeed = std::make_unique<Sel>(sh, "pspeed", "Step length", "Length of one of the 16 steps. 1/16 note makes the pattern one bar long.");
    kPstr = std::make_unique<Knob>(sh, "pstr", "Strength", fmtPct, cMint, kNoMid, "How deep the pattern ducks the sound. 0 is off. Use lightly on full tracks, harder on single loops.", true);
    tPx2 = std::make_unique<Tog>(sh, "px2", "x2", "Doubles the depth of the pattern.");
    setProp(bFlip, "Flip", "Reverses the pattern left to right.");
    for (juce::Component* c : { (juce::Component*) tPon.get(), (juce::Component*) sPspeed.get(), (juce::Component*) kPstr.get(), (juce::Component*) tPx2.get(), (juce::Component*) &bFlip, (juce::Component*) &cPatPre }) content.addAndMakeVisible(*c);
    bFlip.onClick = [this] { float a[16]; for (int i = 0; i < 16; ++i) a[i] = sh.param("pat" + String(i)); for (int i = 0; i < 16; ++i) { sh.gesture("pat" + String(i), true); sh.setParam("pat" + String(i), a[15 - i]); sh.gesture("pat" + String(i), false); } };
    for (int i = 0; i < 5; ++i) cPatPre.addItem(kPats[i].name, i + 1);
    cPatPre.setTextWhenNothingSelected("Pick a rhythm");
    cPatPre.onChange = [this]
    {
        const int i = cPatPre.getSelectedId() - 1; if (i < 0) return;
        for (int s = 0; s < 16; ++s) { sh.gesture("pat" + String(s), true); sh.setParam("pat" + String(s), (float) kPats[i].v[s]); sh.gesture("pat" + String(s), false); }
        cPatPre.setSelectedId(0, juce::dontSendNotification);
    };
    setProp(cPatPre, "Pattern preset", "Ready-made rhythms to start from. Click or drag in the display to draw your own.");
    for (int k = 0; k < 2; ++k)
    {
        const String l = k ? "lb" : "la", n = String(k + 2);
        tLane[k] = std::make_unique<Tog>(sh, l + "_on", "Lane " + n + " on", "Turns lane " + n + " on. It moves the parameter you pick on every step, so the effect itself plays a rhythm.");
        sTgt[k] = std::make_unique<Sel>(sh, l + "_tgt", "Lane " + n + " moves", "The parameter this lane sequences. Mix, outputs and gains move by up to 24 dB at 100 %. Saturation and width sweep their full range.");
        kAmt[k] = std::make_unique<Knob>(sh, l + "_amt", "Amount", [](double v) { return String(v > 0 ? "+" : "") + String(juce::roundToInt(v)) + " %"; }, cMint, 0.f,
                                         "How far the tallest step moves the parameter. Negative flips the direction: tall steps then lower the value.", true);
        bDice[k].setButtonText("Dice"); setProp(bDice[k], "Dice", "Fills lane " + n + " with a random pattern.");
        for (int i = 0; i < 7; ++i) cLanePre[k].addItem(kLanePats[i].name, i + 1);
        cLanePre[k].setTextWhenNothingSelected("Pick a rhythm");
        setProp(cLanePre[k], "Lane " + n + " preset", "Ready-made rhythms for lane " + n + ".");
        cLanePre[k].onChange = [this, l, k]
        {
            const int i = cLanePre[k].getSelectedId() - 1; if (i < 0) return;
            for (int s = 0; s < 16; ++s) { const String id = l + "_s" + String(s); sh.gesture(id, true); sh.setParam(id, (float) kLanePats[i].v[s]); sh.gesture(id, false); }
            cLanePre[k].setSelectedId(0, juce::dontSendNotification);
        };
        bDice[k].onClick = [this, l]
        {
            juce::Random r;
            for (int s = 0; s < 16; ++s) { const String id = l + "_s" + String(s); sh.gesture(id, true); sh.setParam(id, r.nextFloat() < 0.35f ? 0.f : (float) r.nextInt(5)); sh.gesture(id, false); }
        };
        for (juce::Component* c : { (juce::Component*) tLane[k].get(), (juce::Component*) sTgt[k].get(), (juce::Component*) kAmt[k].get(), (juce::Component*) &bDice[k], (juce::Component*) &cLanePre[k] }) content.addAndMakeVisible(*c);
    }
    kGlide = std::make_unique<Knob>(sh, "pglide", "Glide", fmtMs, cMint, kNoMid, "How smoothly lanes 2 and 3 move between steps. Short is sharp and rhythmic, long is a smooth wobble.", true);
    content.addAndMakeVisible(*kGlide);

    selectBand(sh.selb);
    constrainer.setFixedAspectRatio((double) baseW / (double) baseH);
    constrainer.setSizeLimits((int) std::lround(baseW * 0.5), (int) std::lround(baseH * 0.5), baseW * 2, baseH * 2);
    setConstrainer(&constrainer); setResizable(true, false);
    addAndMakeVisible(border); addAndMakeVisible(corner); border.setBorderThickness(juce::BorderSize<int>(6));
    const double sc = juce::jlimit(0.5, 2.0, (double) st.getProperty("uiScale", 1.0));
    setSize((int) std::lround(baseW * sc), (int) std::lround(baseH * sc));
    setView(sh.view);
    applyTips();
    gate.onFrame = [this] { frame(); };
    gate.start(fps);
}
ClawpressorEditor::~ClawpressorEditor() { setLookAndFeel(nullptr); }

void ClawpressorEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform(juce::AffineTransform());
    content.setBounds(0, 0, baseW, baseH);
    content.setTransform(juce::AffineTransform::scale(s));
    border.setBounds(getLocalBounds()); corner.setBounds(getWidth() - 18, getHeight() - 18, 18, 18);
    proc.apvts.state.setProperty("uiScale", (double) s, nullptr);
}

String ClawpressorEditor::presetLabel() const { return proc.presetName(); }

void ClawpressorEditor::applyTips() { applyTipsRec(content, proc.tipsOn()); }

void ClawpressorEditor::setView(int v)
{
    sh.view = juce::jlimit(0, 4, v); proc.apvts.state.setProperty("view", sh.view, nullptr);
    for (int i = 0; i < 5; ++i) tabs[(size_t) i].setToggleState(i == sh.view, juce::dontSendNotification);
    layoutContent(); content.repaint();
}
void ClawpressorEditor::selectBand(int b)
{
    sh.selb = juce::jlimit(0, 3, b); proc.apvts.state.setProperty("selb", sh.selb, nullptr);
    for (int i = 0; i < 4; ++i) bsel[(size_t) i].setToggleState(i == sh.selb, juce::dontSendNotification);
    const String p = "b" + String(sh.selb + 1) + "_"; const Colour c = bandColour(sh.selb);
    kKnee = std::make_unique<Knob>(sh, p + "knee", "Knee", [](double v) { return String(v, 1) + " dB"; }, c, kNoMid, "Width of the soft knee of the selected band: how gently the compression starts around the threshold. Needs 'Soft knee' on. Drag the white dot on the curve too.");
    kUfl = std::make_unique<Knob>(sh, p + "ufl", "Lift floor", [](double v) { return String(v, 1) + " dB"; }, c, kNoMid, "Upward compression: signals quieter than this level get lifted. Move it up to bring out tails and detail. Drag the lower dot on the curve too.");
    kUamt = std::make_unique<Knob>(sh, p + "uamt", "Lift ratio", [](double v) { return v < 1.05 ? String("off") : String(v, 1) + ":1"; }, c, kNoMid, "How strongly quiet signals below the floor are lifted. 1 is off. 2:1 lifts a signal 10 dB under the floor by 5 dB.");
    for (auto* k : { kKnee.get(), kUfl.get(), kUamt.get() }) content.addAndMakeVisible(*k);
    layoutContent(); applyTips(); content.repaint();
}

void ClawpressorEditor::layoutContent()
{
    const int lx = 44, lw = 1026, rx = 1080, rw = 236, v = sh.view, ch = kCtlH[v];
    const int dispY = 130, dispH = 451 - ch, ctlY = dispY + dispH + 8;
    for (int i = 0; i < 5; ++i) tabs[(size_t) i].setBounds(lx + i * 168, 82, 162, 40);
    for (int i = 0; i < 4; ++i) { bsel[(size_t) i].setBounds(934 + i * 35, 86, 31, 32); bsel[(size_t) i].setVisible(v == 1 || v == 2); }
    display.setBounds(lx, dispY, lw, dispH);
    ctlBox = Rectangle<int>(lx, ctlY, lw, ch);
    pPrev.setBounds(364, 22, 30, 30); pName.setBounds(400, 22, 406, 30); pNext.setBounds(812, 22, 30, 30); pSave.setBounds(848, 22, 60, 30); pMore.setBounds(917, 22, 38, 30);
    bGuide.setBounds(971, 22, 70, 30); bSet.setBounds(1051, 22, 93, 30); bAbout.setBounds(1151, 22, 165, 30);
    for (int i = 0; i < 4; ++i) strips[(size_t) i]->setBounds(lx + i * 258, 597, 250, 391);
    mainBox = Rectangle<int>(rx, 82, rw, 236);
    level.setBounds(rx, 326, rw, 662);
    const int kw = (rw - 16) / 3;
    kMix->setBounds(rx + 8, 106, kw, 66); kOut->setBounds(rx + 8 + kw, 106, kw, 66); kPost->setBounds(rx + 8 + 2 * kw, 106, kw, 66);
    kMrel->setBounds(rx + 8, 176, kw, 66);
    tMlim->setBounds(rx + 8, 248, 124, 24); tAutog->setBounds(rx + 8, 276, 90, 24);
    // per view controls
    const int bx = lx + 10, by = ctlY;
    auto show = [&](juce::Component& c, bool on) { c.setVisible(on); };
    for (juce::Component* c : { (juce::Component*) sNb.get(), (juce::Component*) sSlope.get(), (juce::Component*) tRsteps.get(), (juce::Component*) &hintBands }) show(*c, v == 0);
    for (juce::Component* c : { (juce::Component*) kKnee.get(), (juce::Component*) kUfl.get(), (juce::Component*) kUamt.get(), (juce::Component*) &hintCurve }) show(*c, v == 1);
    show(hintScope, v == 2); show(cSpan, v == 3); show(hintGr, v == 3);
    for (juce::Component* c : { (juce::Component*) tPon.get(), (juce::Component*) sPspeed.get(), (juce::Component*) kPstr.get(), (juce::Component*) tPx2.get(), (juce::Component*) &bFlip,
                                (juce::Component*) &cPatPre, (juce::Component*) &hintPat, (juce::Component*) kGlide.get() }) show(*c, v == 4);
    for (int k = 0; k < 2; ++k) for (juce::Component* c : { (juce::Component*) tLane[k].get(), (juce::Component*) sTgt[k].get(), (juce::Component*) kAmt[k].get(), (juce::Component*) &bDice[k], (juce::Component*) &cLanePre[k] }) show(*c, v == 4);
    sNb->setBounds(bx, by + 8, 140, 40); sSlope->setBounds(bx + 154, by + 8, 150, 40); tRsteps->setBounds(bx + 318, by + 15, 110, 28);
    hintBands.setBounds(bx + 440, by, lw - 460, ch);
    kKnee->setBounds(bx, by + 8, 84, 66); kUfl->setBounds(bx + 90, by + 8, 84, 66); kUamt->setBounds(bx + 180, by + 8, 84, 66);
    hintCurve.setBounds(bx + 280, by, lw - 300, ch);
    hintScope.setBounds(bx, by, lw - 20, ch);
    cSpan.setBounds(bx, by + 22, 120, 26); hintGr.setBounds(bx + 140, by, lw - 160, ch);
    const int r0 = by + 8, rh = 50;
    tPon->setBounds(bx, r0 + 8, 96, 28); sPspeed->setBounds(bx + 106, r0 + 2, 190, 40); kPstr->setBounds(bx + 306, r0 + 2, 130, 40); tPx2->setBounds(bx + 442, r0 + 8, 40, 28);
    bFlip.setBounds(bx + 490, r0 + 8, 56, 28); cPatPre.setBounds(bx + 560, r0 + 16, 170, 26); hintPat.setBounds(bx + 740, r0, lw - 760, 44);
    for (int k = 0; k < 2; ++k)
    {
        const int y = r0 + (k + 1) * rh;
        tLane[k]->setBounds(bx, y + 8, 96, 28); sTgt[k]->setBounds(bx + 106, y + 2, 200, 40); kAmt[k]->setBounds(bx + 316, y + 2, 130, 40);
        cLanePre[k].setBounds(bx + 460, y + 16, 170, 26); bDice[k].setBounds(bx + 640, y + 8, 56, 28);
    }
    kGlide->setBounds(bx + 706, r0 + 2 * rh + 2, 130, 40);
    cPatPre.setBounds(bx + 560, r0 + 16, 170, 26);
    if (card) card->setBounds(content.getLocalBounds());
}

void ClawpressorEditor::paintContent(Graphics& g)
{
    const auto b = content.getLocalBounds();
    g.setColour(cPanel); g.fillRect(b);
    g.setGradientFill(juce::ColourGradient(Colour(0xff1e1f23), (float) b.getCentreX(), 0.f, cPanel, (float) b.getCentreX(), 640.f, true)); g.fillRect(b);
    catui::drawWoodCheeks(g, b, cheek);
    if (logo) logo->drawWithin(g, Rectangle<float>(44.f, 12.f, 38.f, 38.f), juce::RectanglePlacement::centred, 1.f);
    T(g, "CLAWPRESSOR", 92.f, 28.f, 0, 1, uiFont(28.f, true, 0.12f), cInk);
    T(g, "STUDIO 87", 93.f, 53.f, 0, 1, uiFont(12.f, false, 0.3f), cYellow);
    g.setColour(cLine); g.drawHorizontalLine(70, 44.f, 1316.f);
    const int v = sh.view;
    if (v == 1 || v == 2) T(g, "BAND", 922.f, 102.f, 2, 1, uiFont(11.f, false, 0.18f), cDim);
    g.setColour(cPanel2); g.fillRoundedRectangle(ctlBox.toFloat(), 4.f); g.setColour(cLine); g.drawRoundedRectangle(ctlBox.toFloat().reduced(0.5f), 4.f, 1.f);
    g.setColour(cPanel2); g.fillRoundedRectangle(mainBox.toFloat(), 4.f); g.setColour(cLine); g.drawRoundedRectangle(mainBox.toFloat().reduced(0.5f), 4.f, 1.f);
    T(g, "MAIN", (float) mainBox.getX() + 10.f, (float) mainBox.getY() + 16.f, 0, 1, uiFont(12.f, false, 0.2f), cDim);
    const int bx = 54, by = ctlBox.getY();
    if (v == 3) T(g, "HISTORY", (float) bx, (float) by + 8.f, 0, 0, uiFont(11.f, false, 0.18f), cDim);
    if (v == 4)
    {
        const int r0 = by + 8;
        T(g, "PATTERN PRESET", (float) bx + 560.f, (float) r0 + 2.f, 0, 0, uiFont(11.f, false, 0.18f), cDim);
        for (int k = 0; k < 2; ++k) T(g, "LANE " + String(k + 2) + " PRESET", (float) bx + 460.f, (float) r0 + (k + 1) * 50.f + 2.f, 0, 0, uiFont(11.f, false, 0.18f), cDim);
        g.setColour(cLine); g.drawHorizontalLine(r0 + 48, (float) bx, (float) ctlBox.getRight() - 10.f);
    }
}

void ClawpressorEditor::frame()
{
    ++frameCount;
    sh.update(1.f / (float) fps);
    if (sh.view == 0) sh.updateSpectrum();
    display.repaint(); level.repaint();
    const int nb = sh.nb();
    for (int b = 0; b < 4; ++b)
    {
        strips[(size_t) b]->grBar.repaint(); strips[(size_t) b]->tick();
        strips[(size_t) b]->setAlpha(b < nb ? 1.f : 0.4f);
        if (frameCount % 4 == 0) strips[(size_t) b]->repaint();
    }
    if (sh.view == 1 && hintCurve.getText() != display.curveInfo) hintCurve.setText(display.curveInfo, juce::dontSendNotification);
    const auto pl = presetLabel(); if (pName.getButtonText() != pl) pName.setButtonText(pl);
}

// ------------------------------------------------------------------ presets
void ClawpressorEditor::stepPreset(int dir)
{
    auto& f = proc.factoryList(); const auto u = proc.userPresetNames(); const int nf = (int) f.size(), N = nf + u.size();
    int cur = 0; const auto name = proc.presetName();
    for (int i = 0; i < nf; ++i) if (f[(size_t) i].name == name) cur = i;
    for (int i = 0; i < u.size(); ++i) if (u[i] == name) cur = nf + i;
    const int n = (cur + dir + N) % N;
    if (n < nf) proc.setCurrentProgram(n); else proc.loadUserPreset(u[n - nf]);
    pName.setButtonText(presetLabel());
}
void ClawpressorEditor::showPresetMenu()
{
    juce::PopupMenu m; auto& f = proc.factoryList(); const auto u = proc.userPresetNames(); const auto cur = proc.presetName();
    m.addSectionHeader("Factory");
    for (size_t i = 0; i < f.size(); ++i) m.addItem(1 + (int) i, f[i].name, true, f[i].name == cur);
    if (u.size() > 0) { m.addSeparator(); m.addSectionHeader("Yours"); for (int i = 0; i < u.size(); ++i) m.addItem(1000 + i, u[i], true, u[i] == cur); }
    juce::Component::SafePointer<ClawpressorEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pName).withMinimumWidth(pName.getWidth()), [safe, u](int r)
    {
        if (!safe || r == 0) return;
        if (r < 1000) safe->proc.setCurrentProgram(r - 1); else safe->proc.loadUserPreset(u[r - 1000]);
        safe->pName.setButtonText(safe->presetLabel());
    });
}
void ClawpressorEditor::showSaveCard()
{
    auto c = std::make_unique<Card>(); c->setContent("Save preset", { { Card::Block::P, "Name your preset. It is stored as a small file in Studio 87/Clawpressor/presets and shows up under Yours." } });
    const auto cur = proc.presetName(); const bool isUser = proc.userPresetNames().contains(cur);
    c->addInput(isUser ? cur : "My preset");
    auto* cp = c.get();
    c->addButton("Cancel", [this] { if (card) card->onClose(); });
    c->addButton("Save", [this, cp] { auto n = cp->inputText().trim(); if (n.isEmpty()) n = "My preset"; proc.saveUserPreset(n); pName.setButtonText(presetLabel()); card->onClose(); }, true);
    showCard(std::move(c));
}
void ClawpressorEditor::showMoreMenu()
{
    juce::PopupMenu m, del; const auto u = proc.userPresetNames();
    for (int i = 0; i < u.size(); ++i) del.addItem(100 + i, u[i]);
    if (u.size() > 0) m.addSubMenu("Delete a saved preset", del); else m.addItem(1, "No saved presets yet", false);
    m.addSeparator();
    m.addItem(2, "Copy my presets to the clipboard (export)", u.size() > 0);
    m.addItem(3, "Paste presets from the clipboard (import)");
    m.addItem(4, "Open the presets folder");
    juce::Component::SafePointer<ClawpressorEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pMore), [safe, u](int r)
    {
        if (!safe || r == 0) return;
        if (r >= 100) safe->proc.deleteUserPreset(u[r - 100]);
        else if (r == 2) safe->exportPresets();
        else if (r == 3) safe->importPresets();
        else if (r == 4) { safe->proc.userDir().createDirectory(); safe->proc.userDir().revealToUser(); }
    });
}
void ClawpressorEditor::exportPresets()
{
    juce::Array<juce::var> all;
    for (auto& n : proc.userPresetNames())
        if (auto x = juce::XmlDocument::parse(proc.userDir().getChildFile(n + ".clawpreset")))
        {
            auto* o = new juce::DynamicObject(); auto* v = new juce::DynamicObject();
            for (int i = 0; i < x->getNumAttributes(); ++i) if (x->getAttributeName(i) != "name") v->setProperty(x->getAttributeName(i), x->getAttributeValue(i).getDoubleValue());
            o->setProperty("name", n); o->setProperty("v", juce::var(v)); all.add(juce::var(o));
        }
    juce::SystemClipboard::copyTextToClipboard(juce::JSON::toString(juce::var(all)));
}
void ClawpressorEditor::importPresets()
{
    const auto parsed = juce::JSON::parse(juce::SystemClipboard::getTextFromClipboard());
    if (!parsed.isArray()) return;
    for (auto& e : *parsed.getArray())
    {
        const String name = e["name"].toString(); auto* vo = e["v"].getDynamicObject(); if (name.isEmpty() || !vo) continue;
        claw::Values vals;
        for (auto& kv : vo->getProperties())
        {
            const String k = kv.name.toString(); float x;
            if (kv.value.isBool()) x = (bool) kv.value ? 1.f : 0.f;
            else if (kv.value.isString())
            {
                const String s = kv.value.toString();
                if (k == "pspeed") x = s == "1/8" ? 1.f : s == "1/4" ? 2.f : 0.f;
                else if (k == "la_tgt" || k == "lb_tgt")
                {
                    if (s == "mix") x = 0; else if (s == "out") x = 1; else if (s == "post") x = 2;
                    else { const int b = s.substring(1, 2).getIntValue(); const String t = s.fromFirstOccurrenceOf("_", false, false); x = (float) (3 + (b - 1) * 4 + (t == "in" ? 0 : t == "out" ? 1 : t == "sat" ? 2 : 3)); }
                }
                else continue;
            }
            else
            {
                x = (float) (double) kv.value;
                if (k == "nb") x -= 1.f; else if (k == "slope") x = x >= 36.f ? 1.f : 0.f;
                else if (k.length() == 6 && k.endsWith("_sat")) x *= 100.f;
            }
            if (proc.apvts.getParameter(k) != nullptr) vals.emplace_back(k, x);
        }
        proc.applyValues(vals, name); proc.saveUserPreset(name);
    }
    pName.setButtonText(presetLabel());
}

// ------------------------------------------------------------------ cards
void ClawpressorEditor::showCard(std::unique_ptr<Card> c)
{
    card = std::move(c);
    juce::Component::SafePointer<ClawpressorEditor> safe(this);
    card->onClose = [safe] { juce::MessageManager::callAsync([safe] { if (safe) safe->closeCard(); }); };
    content.addAndMakeVisible(*card); card->setBounds(content.getLocalBounds()); card->toFront(true); card->grabKeyboardFocus();
}
void ClawpressorEditor::closeCard() { card.reset(); content.repaint(); }
void ClawpressorEditor::showGuide()
{
    using B = Card::Block; auto c = std::make_unique<Card>();
    c->setContent("Quick guide", {
        { B::H, "Signal path" }, { B::P, "Input, crossover into up to 4 bands, per band: input gain, compressor (with automatic makeup), limiter, output. Then the pattern gain, the dry/wet Mix, the Output gain and the master limiter with a post gain." },
        { B::H, "Views" }, { B::Li, "Bands: drag the white crossover lines. Bars show the gain reduction of each band." },
        { B::Li, "Curve: drag the dots: threshold, ratio, knee, and the lift floor and slope (upward compression that lifts quiet parts)." },
        { B::Li, "Band scope: original against processed, per band." }, { B::Li, "GR history: gain reduction over time." },
        { B::Li, "Pattern: three lanes of 16 steps. Lane 1 dips the gain. Lanes 2 and 3 move any parameter you pick, so the effect plays a rhythm." },
        { B::H, "Start here" }, { B::Li, "Pick a preset and keep the input peaking around -6 dB (the marker on the meter)." },
        { B::Li, "Mix at 50 to 75 % keeps the punch (parallel compression)." },
        { B::Li, "The plugin reports 64 samples of latency for its lookahead; your host compensates for it." } });
    c->addButton("Close", [this] { if (card) card->onClose(); }, true); showCard(std::move(c));
}
void ClawpressorEditor::showSettings()
{
    auto c = std::make_unique<Card>(); c->setContent("Settings", { });
    c->addToggleRow("Info bubbles after 1 s of hovering", proc.tipsOn(), [this](bool v) { proc.setTipsOn(v); applyTips(); });
    c->addToggleRow("Eco mode (fewer screen updates, less CPU; audio is not affected)", (bool) proc.apvts.state.getProperty("eco", false),
                    [this](bool v) { proc.apvts.state.setProperty("eco", v, nullptr); fps = v ? 12 : 30; sh.histFps = (float) fps; gate.start(fps); });
    c->setContent("Settings", { { Card::Block::P, "Drawing stops by itself when this window is not in front. Audio, automation and MIDI are not affected." } });
    c->addButton("Close", [this] { if (card) card->onClose(); }, true); showCard(std::move(c));
}
void ClawpressorEditor::showAbout()
{
#ifdef JucePlugin_VersionString
    const String ver = JucePlugin_VersionString;
#else
    const String ver = "?";
#endif
    using B = Card::Block; auto c = std::make_unique<Card>();
    c->setContent("About & Support", {
        { B::P, "Clawpressor " + ver + " is a free and open source plugin by Studio 87, AGPL-3.0-or-later. It collects nothing and never connects to the internet." },
        { B::P, "A four band compressor with a limiter in every band, a master limiter and a pattern gain. Made to make dance music dense and punchy without squashing it." },
        { B::H, "Support" }, { B::P, "If it helps your music, a coffee on Ko-fi keeps new plugins coming." } });
    c->addButton("Support Studio 87 on Ko-fi", [] { juce::URL("https://ko-fi.com/studio87").launchInDefaultBrowser(); });
    c->addButton("GitHub", [] { juce::URL("https://github.com/SpeakingColors/Clawpressor-vst").launchInDefaultBrowser(); });
    c->addButton("Close", [this] { if (card) card->onClose(); }, true); showCard(std::move(c));
}
