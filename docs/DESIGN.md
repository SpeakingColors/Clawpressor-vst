# Design notes

## Signal chain
Input > per band gain > band compressor (autogain) > band limiter > pattern gain (before or after band limiter) > band output > main dry/wet mix > output gain > master limiter > post gain.

- The master limiter sits after the mix so peaks from the dry signal are caught.
- Pattern gain always comes before the master limiter.

## Band section
- 1 to 4 bands. Crossover slope selectable from 6 to 96 dB/oct; 36 or 48 is a sensible start.
- Draggable dividers, bands can be moved without changing width.
- Per band: solo, mute, input level, output level.

## Compressor (per band)
- Threshold, ratio 1:1 to infinity, hard/soft knee, attack, release, boost.
- Release below 1 ms means auto-release, driven by gain reduction.
- Autogain on by default, can be switched off.
- Lookahead from 1 ms to 7.5 ms. Short lookahead with very short release can distort.
- Option to snap ratio to whole numbers.

## Limiter
- Per band limiter with release and its own gain-reduction meter, fixed ratio of infinity.
- Master limiter with release and post gain.

## Main mix
- 0% is dry, 100% is wet. Default 50%. Around 75% is a common parallel compression starting point.

## Pattern gain
- A drawable curve over a 4-beat grid with speed 1/16, 1/8, 1/4, strength 0 to 100% and a double-strength switch.
- Use lightly on full tracks, harder on single loops.

## Views
- Spectrum before (grey) and after (green).
- Per band scope: original, processed, overlap in three colors.
- Gain-reduction history with zoom.
- Density meter view, and a low-CPU scope mode.
