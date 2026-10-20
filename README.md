# Clawpressor 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Free, open source multiband compressor and limiter for Linux (VST3). Up to 4 bands, a compressor and limiter per band, parallel dry/wet mix, drawable rhythmic gain patterns, a master limiter, live scopes and a spectrum view. Dance-ready defaults for house, UK hardcore and hardstyle.

## Status
v0.2.0: the plugin window is built from the approved design mockup (five views: Bands, Curve, Band scope, GR history, Pattern). Compression, per band limiter, saturation, width, pattern gain, two modulation lanes and a 64 sample lookahead master limiter (reported as latency). Every control is a host parameter. 13 factory presets plus your own (Save). Not yet tested in a DAW by the author.

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Features
- 1 to 4 bands, crossover slopes from 6 to 96 dB/oct, draggable band dividers, solo and mute per band
- Per band: threshold, ratio, hard/soft knee, attack, release (auto-release), boost, autogain, limiter, dry/wet
- Main dry/wet mix for parallel ("New York") compression, default 50%
- Rhythmic gain patterns (DPC style): draw a pattern over a 4-beat grid, speed 1/16, 1/8 or 1/4, strength 0 to 100%
- Master output limiter with post gain
- Spectrum before and after, scope and gain-reduction history per band, density meter view, low-CPU scope mode
- Starting presets for dance leads and basses (Dance Master 2 style, low band off) and an OTT-style starter

## Docs
- [Roadmap](ROADMAP.md)
- [Design notes](docs/DESIGN.md)
- [Interactive mockup](docs/mockup/clawpressor.html) (open in a browser; synthetic test signal, not real audio)

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Clawpressor is an independent project. It is not affiliated with or endorsed by any other plugin maker. Names of other products are used only to describe features. No third-party code, artwork or presets are included.
