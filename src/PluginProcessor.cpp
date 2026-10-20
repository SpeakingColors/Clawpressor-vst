#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using Atom = std::atomic<float>;

struct ClawpressorProcessor::Ptrs
{
    Atom *nb, *slope, *x[3], *mix, *out, *post, *mrel, *mlim, *autog, *rsteps, *pon, *pspeed, *pstr, *px2, *pglide, *pat[16];
    struct Band { Atom *on, *solo, *mute, *in, *thr, *ratio, *att, *rel, *autoRel, *soft, *knee, *boost, *lon, *lthr, *lrel, *out, *ufl, *uamt, *sat, *wid; } b[4];
    struct Lane { Atom *on, *tgt, *amt, *st[16]; } lane[2];
};

APVTS::ParameterLayout ClawpressorProcessor::layout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&](const juce::String& id, const juce::String& name, float lo, float hi, float def, float step = 0.f, bool logSkew = false)
    {
        juce::NormalisableRange<float> r(lo, hi, step);
        if (logSkew) r.setSkewForCentre(std::sqrt(lo * hi));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id, 1 }, name, r, def));
    };
    auto T = [&](const juce::String& id, const juce::String& name, bool def)
    { p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, 1 }, name, def)); };
    auto C = [&](const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    { p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id, 1 }, name, items, def)); };
    auto I = [&](const juce::String& id, const juce::String& name, int lo, int hi)
    { p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ id, 1 }, name, lo, hi, 0)); };

    C("nb", "Bands", { "1 band", "2 bands", "3 bands", "4 bands" }, 3);
    C("slope", "Crossover slope", { "24 dB/oct", "48 dB/oct" }, 1);
    F("x1", "Crossover 1", 30.f, 16000.f, 120.f, 1.f, true);
    F("x2", "Crossover 2", 30.f, 16000.f, 1000.f, 1.f, true);
    F("x3", "Crossover 3", 30.f, 16000.f, 6000.f, 1.f, true);
    F("mix", "Mix", 0.f, 100.f, 50.f, 1.f);
    F("out", "Output", -12.f, 12.f, 0.f, 0.1f);
    F("post", "Post gain", -12.f, 6.f, 0.f, 0.1f);
    F("mrel", "Master release", 10.f, 500.f, 80.f, 0.f, true);
    T("mlim", "Master limiter", true);
    T("autog", "Autogain", true);
    T("rsteps", "Whole ratios", false);
    T("pon", "Pattern on", false);
    C("pspeed", "Pattern step length", { "1/16 note (1 bar)", "1/8 note (2 bars)", "1/4 note (4 bars)" }, 0);
    F("pstr", "Pattern strength", 0.f, 100.f, 60.f, 1.f);
    T("px2", "Pattern x2", false);
    F("pglide", "Lane glide", 1.f, 200.f, 12.f, 0.f, true);
    for (int i = 0; i < 16; ++i) I("pat" + juce::String(i), "Pattern step " + juce::String(i + 1), 0, 3);
    const juce::StringArray tg = claw::laneTargetNames();
    const char* ln[2] = { "la", "lb" };
    for (int k = 0; k < 2; ++k)
    {
        const juce::String l = ln[k], n = "Lane " + juce::String(k + 2) + " ";
        T(l + "_on", n + "on", false);
        C(l + "_tgt", n + "target", tg, k ? 9 : 0);
        F(l + "_amt", n + "amount", -100.f, 100.f, k ? 60.f : 50.f, 1.f);
        for (int i = 0; i < 16; ++i) I(l + "_s" + juce::String(i), n + "step " + juce::String(i + 1), 0, 4);
    }
    const char* nm[] = { "Low", "Low mid", "High mid", "High" };
    for (int b = 1; b <= 4; ++b)
    {
        const juce::String s = "b" + juce::String(b) + "_", n = juce::String(nm[b - 1]) + " ";
        T(s + "on", n + "comp", true); T(s + "solo", n + "solo", false); T(s + "mute", n + "mute", false);
        F(s + "in", n + "input", -12.f, 12.f, 0.f, 0.1f);
        F(s + "thr", n + "threshold", -60.f, 0.f, -18.f, 0.5f);
        F(s + "ratio", n + "ratio", 1.f, 20.f, 3.f, 0.1f);
        F(s + "att", n + "attack", 0.1f, 100.f, 10.f, 0.f, true);
        F(s + "rel", n + "release", 5.f, 2000.f, 120.f, 0.f, true);
        T(s + "auto", n + "auto release", false); T(s + "soft", n + "soft knee", true);
        F(s + "knee", n + "knee", 0.f, 24.f, 6.f, 0.5f);
        F(s + "boost", n + "boost", 0.f, 12.f, 0.f, 0.1f);
        T(s + "lon", n + "limiter", true);
        F(s + "lthr", n + "limit", -24.f, 0.f, -3.f, 0.1f);
        F(s + "lrel", n + "limiter release", 5.f, 500.f, 60.f, 0.f, true);
        F(s + "out", n + "output", -12.f, 12.f, 0.f, 0.1f);
        F(s + "ufl", n + "lift floor", -60.f, -20.f, -60.f, 0.5f);
        F(s + "uamt", n + "lift ratio", 1.f, 4.f, 1.f, 0.1f);
        F(s + "sat", n + "saturation", 0.f, 100.f, 0.f, 1.f);
        F(s + "wid", n + "width", 0.f, 200.f, 100.f, 1.f);
    }
    return { p.begin(), p.end() };
}

ClawpressorProcessor::ClawpressorProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", layout()), ptrs(std::make_unique<Ptrs>())
{
    auto R = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };
    auto& q = *ptrs;
    q.nb = R("nb"); q.slope = R("slope"); q.mix = R("mix"); q.out = R("out"); q.post = R("post"); q.mrel = R("mrel"); q.mlim = R("mlim"); q.autog = R("autog");
    q.rsteps = R("rsteps"); q.pon = R("pon"); q.pspeed = R("pspeed"); q.pstr = R("pstr"); q.px2 = R("px2"); q.pglide = R("pglide");
    for (int i = 0; i < 3; ++i) q.x[i] = R("x" + juce::String(i + 1));
    for (int i = 0; i < 16; ++i) q.pat[i] = R("pat" + juce::String(i));
    for (int k = 0; k < 2; ++k)
    {
        const juce::String l = k ? "lb" : "la";
        q.lane[k].on = R(l + "_on"); q.lane[k].tgt = R(l + "_tgt"); q.lane[k].amt = R(l + "_amt");
        for (int i = 0; i < 16; ++i) q.lane[k].st[i] = R(l + "_s" + juce::String(i));
    }
    for (int b = 0; b < 4; ++b)
    {
        const juce::String s = "b" + juce::String(b + 1) + "_"; auto& d = q.b[b];
        d.on = R(s + "on"); d.solo = R(s + "solo"); d.mute = R(s + "mute"); d.in = R(s + "in"); d.thr = R(s + "thr"); d.ratio = R(s + "ratio"); d.att = R(s + "att");
        d.rel = R(s + "rel"); d.autoRel = R(s + "auto"); d.soft = R(s + "soft"); d.knee = R(s + "knee"); d.boost = R(s + "boost"); d.lon = R(s + "lon");
        d.lthr = R(s + "lthr"); d.lrel = R(s + "lrel"); d.out = R(s + "out"); d.ufl = R(s + "ufl"); d.uamt = R(s + "uamt"); d.sat = R(s + "sat"); d.wid = R(s + "wid");
    }
    setLatencySamples(claw::Engine::kLook);
    setCurrentProgram(4);   // Hardstyle Lock, like the mockup
}
ClawpressorProcessor::~ClawpressorProcessor() = default;

bool ClawpressorProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void ClawpressorProcessor::prepareToPlay(double sr, int bs)
{
    fs = sr; engine.prepare(sr); freeBeat = 0.0;
    tmpR.assign((size_t) juce::jmax(1, bs), 0.f);
    setLatencySamples(claw::Engine::kLook);
}

void ClawpressorProcessor::pullParams()
{
    auto& P = engine.p; auto& q = *ptrs;
    const int nb = (int) q.nb->load() + 1, slope = q.slope->load() > 0.5f ? 48 : 24;
    float x[3] = { q.x[0]->load(), q.x[1]->load(), q.x[2]->load() };
    x[1] = juce::jmax(x[1], x[0] * 1.2f); x[2] = juce::jmax(x[2], x[1] * 1.2f);
    if (nb != P.nb || slope != P.slope || x[0] != P.x[0] || x[1] != P.x[1] || x[2] != P.x[2]) engine.markDirty();
    P.nb = nb; P.slope = slope; for (int i = 0; i < 3; ++i) P.x[i] = x[i];
    P.mix = q.mix->load(); P.out = q.out->load(); P.post = q.post->load(); P.mrel = q.mrel->load();
    P.mlim = q.mlim->load() > 0.5f; P.autog = q.autog->load() > 0.5f; P.rsteps = q.rsteps->load() > 0.5f;
    P.pon = q.pon->load() > 0.5f; P.pspeed = juce::jlimit(0, 2, (int) q.pspeed->load()); P.pstr = q.pstr->load(); P.px2 = q.px2->load() > 0.5f; P.glide = q.pglide->load();
    for (int i = 0; i < 16; ++i) P.pat[i] = (int) q.pat[i]->load();
    for (int k = 0; k < 2; ++k)
    {
        auto& L = P.lane[(size_t) k]; auto& d = q.lane[k];
        L.on = d.on->load() > 0.5f; L.tgt = juce::jlimit(0, 18, (int) d.tgt->load()); L.amt = d.amt->load();
        for (int i = 0; i < 16; ++i) L.st[i] = (int) d.st[i]->load();
    }
    for (int b = 0; b < 4; ++b)
    {
        auto& B = P.b[(size_t) b]; auto& d = q.b[b];
        B.on = d.on->load() > 0.5f; B.solo = d.solo->load() > 0.5f; B.mute = d.mute->load() > 0.5f; B.in = d.in->load(); B.thr = d.thr->load(); B.ratio = d.ratio->load();
        B.att = d.att->load(); B.rel = d.rel->load(); B.autoRel = d.autoRel->load() > 0.5f; B.soft = d.soft->load() > 0.5f; B.knee = d.knee->load(); B.boost = d.boost->load();
        B.lim = d.lon->load() > 0.5f; B.lthr = d.lthr->load(); B.lrel = d.lrel->load(); B.out = d.out->load(); B.ufl = d.ufl->load(); B.uamt = d.uamt->load();
        B.sat = d.sat->load(); B.wid = d.wid->load();
    }
}

void ClawpressorProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    if (nch < 1 || n < 1) return;
    pullParams();
    double bpm = 120.0, beat0 = freeBeat; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto t = pos->getBpm()) bpm = *t;
            playing = pos->getIsPlaying();
            if (playing) if (auto q = pos->getPpqPosition()) beat0 = *q;
        }
    hostBpm = bpm; hostPlaying = playing;
    float* L = buf.getWritePointer(0);
    float* R = nch > 1 ? buf.getWritePointer(1) : nullptr;
    if (!R) { if ((int) tmpR.size() < n) tmpR.assign((size_t) n, 0.f); std::copy(L, L + n, tmpR.begin()); R = tmpR.data(); }
    engine.process(L, R, n, beat0, bpm);
    if (nch < 2) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
    freeBeat = beat0 + (double) n * bpm / 60.0 / fs;
}

juce::AudioProcessorEditor* ClawpressorProcessor::createEditor() { return new ClawpressorEditor(*this); }

// ---------------- presets
void ClawpressorProcessor::applyValues(const claw::Values& v, const juce::String& name)
{
    for (auto& kv : v)
        if (auto* q = apvts.getParameter(kv.first))
        {
            q->beginChangeGesture(); q->setValueNotifyingHost(q->convertTo0to1(kv.second)); q->endChangeGesture();
        }
    apvts.state.setProperty("preset", name, nullptr);
}
void ClawpressorProcessor::setCurrentProgram(int i)
{
    if (juce::isPositiveAndBelow(i, (int) factory.size())) { program = i; applyValues(factory[(size_t) i].v, factory[(size_t) i].name); }
}
juce::File ClawpressorProcessor::userDir() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio 87").getChildFile("Clawpressor").getChildFile("presets");
}
juce::StringArray ClawpressorProcessor::userPresetNames() const
{
    juce::StringArray a;
    for (auto& f : userDir().findChildFiles(juce::File::findFiles, false, "*.clawpreset")) a.add(f.getFileNameWithoutExtension());
    a.sort(true); return a;
}
static juce::String safeName(const juce::String& n) { return juce::File::createLegalFileName(n.trim()).substring(0, 60); }
bool ClawpressorProcessor::saveUserPreset(const juce::String& name)
{
    const auto s = safeName(name); if (s.isEmpty()) return false;
    auto dir = userDir(); if (!dir.createDirectory()) return false;
    juce::XmlElement x("ClawPreset"); x.setAttribute("name", s);
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm))
            x.setAttribute(r->paramID, (double) r->convertFrom0to1(r->getValue()));
    if (!x.writeTo(dir.getChildFile(s + ".clawpreset"))) return false;
    apvts.state.setProperty("preset", s, nullptr); return true;
}
bool ClawpressorProcessor::loadUserPreset(const juce::String& name)
{
    auto f = userDir().getChildFile(safeName(name) + ".clawpreset");
    if (auto x = juce::XmlDocument::parse(f))
    {
        claw::Values v;
        for (int i = 0; i < x->getNumAttributes(); ++i) { const auto k = x->getAttributeName(i); if (k != "name") v.emplace_back(k, (float) x->getAttributeValue(i).getDoubleValue()); }
        applyValues(v, safeName(name)); return true;
    }
    return false;
}
bool ClawpressorProcessor::deleteUserPreset(const juce::String& name) { return userDir().getChildFile(safeName(name) + ".clawpreset").deleteFile(); }

void ClawpressorProcessor::getStateInformation(juce::MemoryBlock& d)
{ if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d); }
void ClawpressorProcessor::setStateInformation(const void* d, int s)
{ if (auto xml = getXmlFromBinary(d, s)) if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ClawpressorProcessor(); }
