# AudioFilePlayer

A minimal utility plugin: loads an audio file and plays it back through its
output. No audio input, no MIDI — just Load / Play / Stop / Loop and a gain
knob. Meant as a test-signal source you can wire into an effect plugin (e.g.
StringLife) inside a host like JUCE's AudioPluginHost, so you can audition an
effect against a real recording without needing a full DAW project.

## Formats

VST3, AU, Standalone (built with JUCE 8). Registers as a generator/instrument
category (`kAudioUnitType_Generator` / VST3 "Generator") rather than an
effect, since it has no audio input.

## Building

```
cmake -B build
cmake --build build --config Release
```

Requires CMake 3.22+. JUCE is fetched automatically via `FetchContent` (tag
`8.0.4`).

## Usage

1. Click **Load...** and pick a `.wav`/`.aiff`/etc. file.
2. Click **Play** to start playback; **Stop** to halt it.
3. **Loop** toggles seamless looping.
4. **Gain** trims the output level.
5. Drag the timeline to seek anywhere in the file.

In AudioPluginHost: add this plugin's node and an effect plugin's node to the
graph, connect this node's audio output to the effect's audio input, and the
effect's output to Audio Output (Internal). No MIDI routing needed.

## UI Reference

- **File name label** — shows the loaded file's name, or "No file loaded".
- **Timeline / scrub bar** — draggable position slider spanning the full
  length of the loaded file. Dragging seeks live; while playing, the thumb
  also advances on its own (position polled at 15 Hz). Your drag always wins
  over the automatic update — the plugin skips the periodic refresh while
  you're holding the slider down.
- **Load...** — opens a file picker (`.wav`/`.aif`/`.aiff`/`.mp3`/`.flac`/
  `.ogg`) to load a new source file.
- **Play** — starts playback from the current position (or from 0 if the
  file had already played to the end).
- **Stop** — halts playback and resets the position to 0.
- **Loop** — toggles seamless looping (wraps at end-of-file with no gap).
- **Position label** — "current / total" time, `m:ss` format.
- **Gain knob** — output level trim, linear 0–1, displayed to 2 decimal
  places.
