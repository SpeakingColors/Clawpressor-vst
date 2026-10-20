// Clawpressor editor: built from the approved design mockup (docs/mockup/clawpressor.html), 1360 x 1000 design size.
#pragma once
#include "PluginProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>
#include <functional>
#include <set>

namespace clawui
{
using juce::Colour;
using juce::String;

inline const Colour cGround { 0xff0b0b0d }, cPanel { 0xff15161a }, cPanel2 { 0xff1c1d22 }, cLine { 0xff2b2d34 }, cInk { 0xfff4f5f7 }, cDim { 0xffa9adb8 },
                    cDisp { 0xff06080a }, cMint { 0xff02EBBA }, cPink { 0xffFF1383 }, cPurple { 0xffA43DFF }, cYellow { 0xffFFD100 },
                    cPressed { 0xff0f2a26 }, cField { 0xff0a0a0c }, cGridLine { 0xff1a2422 }, cAxisX { 0xff6fd6c4 }, cAxisY { 0xff8f8bd6 };
constexpr float kNoMid = -1.0e9f;
inline Colour bandColour(int i) { static const Colour c[4] = { cMint, cYellow, cPink, cPurple }; return c[i & 3]; }
inline const char* bandName(int i) { static const char* n[4] = { "Low", "Low mid", "High mid", "High" }; return n[i & 3]; }
inline juce::Font uiFont(float h, bool bold = false, float kern = 0.08f)
{ juce::Font f(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); f.setExtraKerningFactor(kern); return f; }
inline float strW(const juce::Font& f, const String& t) { return juce::GlyphArrangement::getStringWidth(f, t); }
inline juce::Font monoFont(float h)
{ return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain)); }

String fmtDb(double v);
String fmtMs(double v);
String fmtHz(double v);
String fmtPct(double v);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const Colour&, bool hi, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool hi, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return uiFont(14.f, false, 0.03f); }
    juce::Font getPopupMenuFont() override { return uiFont(15.f, false, 0.02f); }
    juce::Font getLabelFont(juce::Label& l) override { return l.getFont(); }
    juce::Rectangle<int> getTooltipBounds(const String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;
    void drawTooltip(juce::Graphics&, const String& text, int w, int h) override;
};

// State shared by the display, the meters and the band strips (all read on the message thread)
struct Shared
{
    explicit Shared(ClawpressorProcessor& p);
    ClawpressorProcessor& proc;
    int view = 0, selb = 0;           // views: bands, curve, scope, gr, pattern
    float grSpan = 10.f, histFps = 30.f;
    bool density = false;
    float inL = -90, inR = -90, outL = -90, outR = -90, hInL = -90, hInR = -90, hOutL = -90, hOutR = -90, rms = -90, crest = 0, dens = 0, clipT = 0;
    float gr[4] = {}, lgr[4] = {}, lvl[4] = { -90, -90, -90, -90 }, mgr = 0;
    int step = -1; bool active = false;
    std::vector<float> hist[5];
    std::vector<float> specPre, specPost;
    juce::dsp::FFT fft { 12 };
    std::vector<float> win, work;
    void update(float dt);
    void updateSpectrum();
    float param(const String& id) const { return proc.get(id); }
    void setParam(const String& id, float plain);
    void gesture(const String& id, bool begin);
    int nb() const { return (int) param("nb") + 1; }
};

// Rotary knob with a name and an editable value under it (double click the value to type).
class Knob : public juce::Component
{
public:
    Knob(Shared&, const String& id, const String& label, std::function<String(double)> fmt, Colour arc, float mid, const String& tip, bool side = false, float scale = 0.88f);
    void resized() override;
    void paint(juce::Graphics&) override;
    void setTip(const String& t);
    void refresh();
    String tipText;
    juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Label val;
private:
    Shared& sh; String id, name; std::function<String(double)> fmt; bool side; float scale;
    std::unique_ptr<juce::SliderParameterAttachment> att;
};

class Tog : public juce::TextButton
{
public:
    Tog(Shared&, const String& id, const String& text, const String& tip);
private:
    std::unique_ptr<juce::ButtonParameterAttachment> att;
};

class Sel : public juce::Component
{
public:
    Sel(Shared&, const String& id, const String& label, const String& tip);
    void resized() override;
    void paint(juce::Graphics&) override;
    juce::ComboBox box;
    String name;
private:
    std::unique_ptr<juce::ComboBoxParameterAttachment> att;
};

class GrBar : public juce::Component
{
public:
    GrBar(Shared& s, int b) : sh(s), band(b) {}
    void paint(juce::Graphics&) override;
private:
    Shared& sh; int band;
};

class BandStrip : public juce::Component
{
public:
    BandStrip(Shared&, int band);
    void paint(juce::Graphics&) override;
    void resized() override;
    void tick();
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Tog>> togs;   // S, M, Comp, Auto rel, Soft knee, Limiter
    GrBar grBar;
private:
    Shared& sh; int b;
};

class DisplayView : public juce::Component
{
public:
    explicit DisplayView(Shared& s) : sh(s) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    String curveInfo;
private:
    void touch(const String&, float);
    void endTouches();
    std::vector<std::pair<String, juce::Point<float>>> handles;
    std::set<String> touched;
    void drawBands(juce::Graphics&, float w, float h);
    void drawCurve(juce::Graphics&, float w, float h);
    void drawScope(juce::Graphics&, float w, float h);
    void drawGR(juce::Graphics&, float w, float h);
    void drawPat(juce::Graphics&, float w, float h);
    void curvePtr(juce::Point<float>, bool down);
    void patPtr(juce::Point<float>, bool down);
    int hitX(float x) const;
    Shared& sh;
    int xDrag = -1, xHover = -1, patLane = 0; bool patPaint = false;
    String cDrag;
    struct Geo { float x0 = 0, x1 = 0, y0 = 0, y1 = 0, D0 = -60, D1 = 6; } geo;
    struct PGeo { float ml = 96, mt = 6, cw = 0, lh = 0, gap = 8, w = 0, h = 0; } pgeo;
    int beginnedGesture = 0; String gestureId;
};

class LevelPanel : public juce::Component
{
public:
    explicit LevelPanel(Shared&);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::TextButton modeBtn { "Density" };
private:
    Shared& sh;
    juce::Rectangle<int> cv;
};

class Card : public juce::Component
{
public:
    Card();
    struct Block { enum Kind { H, P, Li } kind; String text; };
    void setContent(const String& title, std::vector<Block> blocks);
    void addButton(const String& text, std::function<void()> fn, bool primary = false);
    void addInput(const String& initial);
    void addToggleRow(const String& text, bool initial, std::function<void(bool)> fn);
    String inputText() const { return input ? input->getText() : String(); }
    std::function<void()> onClose;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    String title; std::vector<Block> blocks; juce::TextLayout layout;
    std::vector<std::unique_ptr<juce::TextButton>> buttons; std::unique_ptr<juce::TextEditor> input;
    struct TRow { String text; std::unique_ptr<juce::TextButton> b; };
    std::vector<std::unique_ptr<TRow>> rows;
    juce::Rectangle<int> box;
};
}

class ClawpressorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ClawpressorEditor(ClawpressorProcessor&);
    ~ClawpressorEditor() override;
    void resized() override;
    static constexpr int baseW = 1360, baseH = 1000, cheek = 30;
private:
    struct Content : juce::Component
    {
        explicit Content(ClawpressorEditor& o) : ed(o) {}
        void paint(juce::Graphics& g) override { ed.paintContent(g); }
        void resized() override { ed.layoutContent(); }
        void mouseDoubleClick(const juce::MouseEvent& e) override { if (e.x < cheek || e.x >= baseW - cheek) ed.setSize(baseW, baseH); }
        ClawpressorEditor& ed;
    };
    void paintContent(juce::Graphics&);
    void layoutContent();
    void setView(int);
    void selectBand(int);
    void applyTips();
    void frame();
    void importPresets();
    void exportPresets();
    void showPresetMenu();
    void showMoreMenu();
    void stepPreset(int dir);
    void showSaveCard();
    void showCard(std::unique_ptr<clawui::Card>);
    void closeCard();
    void showGuide();
    void showSettings();
    void showAbout();
    juce::String presetLabel() const;

    ClawpressorProcessor& proc;
    clawui::Look look;
    Content content { *this };
    clawui::Shared sh;
    catui::RenderGate gate { *this };
    juce::TooltipWindow tooltipWindow { this, 1000 };
    clawui::DisplayView display;
    clawui::LevelPanel level;
    std::array<std::unique_ptr<clawui::BandStrip>, 4> strips;
    std::array<juce::TextButton, 5> tabs;
    std::array<juce::TextButton, 4> bsel;
    juce::TextButton pPrev { juce::CharPointer_UTF8("\xe2\x80\xb9") }, pName, pNext { juce::CharPointer_UTF8("\xe2\x80\xba") }, pSave { "Save" }, pMore { juce::CharPointer_UTF8("\xe2\x80\xa6") },
                     bGuide { "Guide" }, bSet { "Settings" }, bAbout { "About & Support" };
    std::unique_ptr<clawui::Knob> kMix, kOut, kPost, kMrel, kPstr, kGlide, kAmt[2];
    std::unique_ptr<clawui::Tog> tMlim, tAutog, tRsteps, tPon, tPx2, tLane[2];
    std::unique_ptr<clawui::Sel> sNb, sSlope, sPspeed, sTgt[2];
    juce::ComboBox cSpan;
    juce::TextButton bFlip { "Flip" }, bDice[2] { juce::TextButton("Dice"), juce::TextButton("Dice") };
    juce::ComboBox cPatPre, cLanePre[2];
    juce::Label hintBands, hintCurve, hintScope, hintGr, hintPat;
    std::unique_ptr<clawui::Knob> kKnee, kUfl, kUamt;     // curve view: knobs of the selected band
    std::unique_ptr<clawui::Card> card;
    juce::Rectangle<int> mainBox, ctlBox;
    std::unique_ptr<juce::Drawable> logo;
    int fps = 30;
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableBorderComponent border { this, &constrainer };
    juce::ResizableCornerComponent corner { this, &constrainer };
    int lastCtlH = -1, frameCount = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClawpressorEditor)
};
