# Roadmap: Clawpressor

Multiband compressor and limiter. The same engine will also power the Dance MB section inside SupurrSaw.

## Next
- [x] Mockup page on the cat-ui look (wood cheeks, resizable, reset to 100%): 4 bands, crossovers, per band compressor and limiter, curve, band scope, GR history, pattern gain, master limiter, density meter (v2, 2026-10-19)
- [x] DSP core in C++ (include/claw, engine and DSP tests pass): crossover (selectable slope), per band compressor with auto-release and autogain, per band limiter, master limiter, dry/wet
- [x] JUCE VST3 shell on cat-ui, draws through `catui::RenderGate`
- [x] Spectrum and per band scope, gain-reduction history, density meter
- [ ] Low-CPU scope mode (the README lists it; I could not find it in the editor code)
- [x] Pattern gain module (DPC style) with a pattern editor
- [ ] Preset patterns (not checked)
- [x] Presets (Neutral Glue, Dance Glue, Hardstyle Lock, UK Hardcore Punch and an OTT style starter): Dance Master 2 style (low band off), OTT-style starter for dance, UK hardcore and hardstyle, neutral glue
- [x] Input level marker near -6 dB for easy gain staging
- [x] CI: Linux build and pluginval on every push; Windows and macOS are built by the release workflow (installers and PDF manual)

## Rules for every cat plugin
- Free, open source, AGPL-3.0-or-later
- Light on CPU, RAM and GPU; stop drawing when the window is not in front, audio keeps running
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%

- [x] Interactive mockup page (docs/mockup/clawpressor.html)
- [x] JUCE scaffold on cat-ui builds in CI (Linux VST3 + standalone)
- [x] Real editor from the mockup (spectrum, curves, pattern editor), phase-aligned crossover, pattern gain, 64 sample lookahead reported as latency
- [ ] Listen and tune by ear; test in a real DAW
