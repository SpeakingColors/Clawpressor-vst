# Roadmap: Clawpressor

Multiband compressor and limiter. The same engine will also power the Dance MB section inside SupurrSaw.

## Next
- [ ] Mockup page on the cat-ui look (wood cheeks, resizable, reset to 100%)
- [ ] DSP core in C++: crossover (selectable slope), per band compressor with auto-release and autogain, per band limiter, master limiter, dry/wet
- [ ] JUCE VST3 shell on cat-ui, draw through `catui::RenderGate`
- [ ] Spectrum and per band scope, gain-reduction history, density meter, low-CPU scope mode
- [ ] Pattern gain module (DPC style) with a pattern editor and preset patterns
- [ ] Presets: Dance Master 2 style (low band off), OTT-style starter for dance, UK hardcore and hardstyle, neutral glue
- [ ] Input level marker near -6 dB for easy gain staging
- [ ] CI on Linux, Windows and macOS (same as cat-ui)

## Rules for every cat plugin
- Free, open source, AGPL-3.0-or-later
- Light on CPU, RAM and GPU; stop drawing when the window is not in front, audio keeps running
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%

- [x] Interactive mockup page (docs/mockup/clawpressor.html)
- [ ] JUCE scaffold on cat-ui (processor, 4-band crossover, editor)
