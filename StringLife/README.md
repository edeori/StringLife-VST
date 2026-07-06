# StringLife

StringLife is a guitar and bass string restoration effect plugin. It processes
real recorded guitar/bass audio and makes old, dead strings sound fresher and
more alive — without adding string noise, reverb, or delay.

## Formats

VST3, AU, Standalone (built with JUCE 8).

## Building

```
cmake -B build
cmake --build build --config Release
```

Requires CMake 3.22+. JUCE is fetched automatically via `FetchContent` (tag
`8.0.4`) — no manual JUCE checkout needed.

On macOS the build targets `arm64;x86_64` with a deployment target of macOS
12.0.

## Parameters

- **String Age** — master macro (0 = Dead, 1 = Fresh) driving four advanced
  parameters shown on the knobs as Spark, Bite, Shimmer and Bloom (internally
  still Presence/Attack Restore/Harmonic Excite/Sustain Shape). Each
  advanced parameter tracks the macro via a signed offset (its value minus
  the macro's value, captured the moment you last touched it by hand) rather
  than a one-shot copy — moving the macro shifts every parameter by the same
  amount, preserving whatever manual differences you've dialled in, like a
  synth macro with relative/offset mapping. A parameter clamps at 0%/100% if
  the macro pushes it past its range, but resumes tracking as soon as the
  macro moves back.
- **Instrument** — Guitar or Bass, changes the frequency focus of the
  restoration DSP (exact bands below under UI Reference).
- **Wet/Dry Mix** — only active in NORMAL monitor mode.
- **Monitor Mode** — NORMAL (processed output) or DELTA (wet minus dry,
  +6dB makeup). A/B and Bypass are separate controls, not monitor modes —
  see UI Reference.

## Signal chain

```
Input -> Transient Enhancer -> Harmonic Exciter -> Presence EQ -> Upward Expander -> Wet/Dry mix -> Output
```

All continuously variable amounts are smoothed (sample-rate aware, no
allocation or locking on the audio thread), so host automation and fast knob
moves - including DEAD↔FRESH macro sweeps, which fan out to the four advanced
parameters - don't produce zipper noise: the three envelope/waveshaper stages
smooth their amounts per-sample over ~20 ms, the wet/dry mix crossfades
per-sample over ~30 ms, and the presence shelf ramps its gain over ~50 ms with
allocation-free coefficient updates at block rate while the value is moving.

## UI Reference

### Top bar
- **StringLife** title — branding only, no function.
- **Guitar / Bass** — sets the `Instrument` parameter, which changes the
  frequency focus of every DSP stage below:
  | | Guitar | Bass |
  |---|---|---|
  | Enhancement focus | 2 kHz – 12 kHz | 800 Hz – 6 kHz |
  | Presence shelf | 5 kHz, 0 to +6 dB | 2.5 kHz, 0 to +5 dB |
  | Harmonic Exciter high-pass | 2 kHz | 800 Hz |

### Main macro
- **DEAD ↔ FRESH slider** (`String Age`, 0–1) — see Parameters above for the
  exact offset-tracking mechanism. Current value shown below as "NN% Fresh".

### Advanced row
Each slider's value is shown below it as a percentage (0–100%).
- **Spark** (Presence) — high-shelf boost. See the Guitar/Bass table above
  for frequency and max gain; filter Q is fixed at 0.707 (no resonant peak).
- **Bite** (Attack Restore) — transient enhancer. Envelope follower with
  0.5 ms attack / 20 ms release; rising transients are boosted by a gain
  multiply of up to `amount × 8` on top of the signal.
  <br>*TODO: naive envelope-derivative detector, no lookahead — see
  `TransientEnhancer.h`.*
- **Shimmer** (Harmonic Excite) — harmonic exciter. A one-pole high-pass
  copy of the signal (cutoff per the Guitar/Bass table above) is driven
  through a tanh soft-clipper — drive scales up to ×3 at full amount,
  calibrated for ~0.3% THD at `amount = 1` — and mixed back in at up to 30%
  of the driven signal's level.
- **Bloom** (Sustain Shape) — gentle upward expander on the decaying tail.
  Threshold fixed at −30 dBFS; below threshold, the distance to the
  threshold is compressed by a ratio of up to 1.2:1, i.e. quiet tails are
  pulled up towards −30 dB instead of decaying naturally. Envelope: 5 ms
  attack / 50 ms release.
  <br>*TODO: simple peak envelope, not RMS — see `UpwardExpander.h`.*

### Monitor section
- **NORMAL / DELTA** — mutually exclusive monitor mode:
  - NORMAL: dry/wet blend controlled by the MIX knob.
  - DELTA: outputs `(wet − dry) × 2` (+6 dB makeup gain) — isolates exactly
    what the plugin is adding; ignores the MIX knob.
- **BYPASS** — hard, host-recognized bypass (also exposed as the plugin's
  `getBypassParameter()`, so a host's own bypass button/automation works
  too). Fully skips all DSP — output is the unprocessed input — and disables
  every other control in this section, since nothing else has an effect
  while bypassed.
- **A / B** — quick-compare toggle between two independent, full settings
  snapshots, *not* a dry/bypass switch. Flipping it saves every "sound"
  parameter (String Age, Instrument, all four advanced parameters, and the
  MIX knob) into the side you're leaving, then recalls the other side's
  stored values — both sides start out identical, and diverge as you tweak
  each one. The button label shows which side ("A" or "B") is currently
  live.

### MIX knob
- **Wet/Dry Mix** (0–100%) — only enabled in NORMAL monitor mode; dimmed
  (but still visible) otherwise since DELTA and BYPASS override it.

### Meters
- **IN / OUT** — vertical peak-hold meters, −60 dB to 0 dB, refreshed at
  30 Hz. The white line is the peak-hold indicator: holds for ~1 second,
  then decays at 0.5 dB per tick (~15 dB/sec) until a new higher peak
  arrives.

## UI assets & fonts

The panel background, button on/off states, knob face texture and the
DEAD↔FRESH slider thumb are real rendered images (`Assets/*.png`), embedded
via `juce_add_binary_data`. The rotary knobs use the image only for the
static metal face — ticks, glow arc and pointer are drawn in code so every
value renders correctly, since only one fixed-rotation render of the knob
exists (see the comment in `StringLifeLookAndFeel::drawRotarySlider`).

Typography is three embedded Google Fonts (OFL-licensed, see
`Assets/Fonts/OFL-*.txt`), not system fonts, so it looks the same on every
machine: **Oxanium** (title), **Barlow Condensed** (everything else),
**Rajdhani** (numeric readouts) — see `StringLifeFonts.h`.

## Status

DSP stages are currently placeholder-quality (see `TODO` comments in
`Source/`) — interfaces are stable but the algorithms need tuning against real
guitar/bass recordings.
