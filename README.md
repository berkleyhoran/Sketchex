# Sketchex

Draw your melody. A gexex **MIDI-out sequencer** — VST3 (also AU on macOS,
and Standalone). Sketch a line on the canvas; a playhead sweeps it left to
right in time with your DAW and plays scale-locked notes from the height of
your drawing into whatever synth you put after it.

Sibling of [Synthex](https://github.com/berkleyhoran/gexexsynth) and
[Reactex](https://github.com/berkleyhoran/Kaleidosonic).

## Download

Grab the latest installer for your platform from the
**[Releases page](https://github.com/berkleyhoran/Sketchex/releases)**:

- **Windows** — run the installer; it drops the VST3 into
  `Program Files\Common Files\VST3` (rescan plugins in your DAW if it
  doesn't show up right away).
- **macOS** — unzip, drag the `.vst3`/`.component` into your usual plugin
  folders. Unsigned/un-notarized for now, so Gatekeeper will ask you to
  right-click → Open the first time.
- **Linux** — untar and copy the `.vst3` into `~/.vst3`.

## How it works

- **Canvas** = time (left→right, one loop) × pitch (bottom→top).
  Left-drag draws *anything* — lines, squiggles, circles, going backwards —
  right-drag / `E` erases, `Ctrl+Z` / `Ctrl+Y` undo/redo. Wherever the
  playhead crosses ink it plays a note, so overlapping strokes and shapes
  that cross the playhead twice (a circle) play as chords.
- **Root / Scale** — every lane on the canvas is a note in the chosen
  scale, so anything you draw is in key. 14 scales from Major to Chromatic.
- **View Oct / View Rng** — the canvas is a window onto the full C0–C8
  range: mouse-wheel scrolls, Ctrl+wheel zooms, or drag the scrollbar.
  Drawings keep their absolute pitch when you scroll.
- **Length** (½–16 bars) and **Rate** (1/1 … 1/64 incl. triplets and
  dotted) — the loop length and the grid notes fire on. **Swing** delays
  every second step.
- **Notes** — **Retrig** fires a new note on every grid step while the
  line is under the playhead (**Gate** sets how long each is held);
  **Hold** presses the note down at the first step it meets ink and keeps
  holding until the line ends or moves to a new lane.
- Strokes that start near the left/right edge snap to it, so a note on
  beat 1 is easy to hit.
- **Glide** — how much the pitch follows the *exact* curve you drew between
  scale notes. Two modes:
  - **Bend** — continuous pitch-bend that traces your line. Set **Bend Rng**
    to match the synth's pitch-bend range (12 st default).
  - **Legato** — overlapping note-on/off plus CC 5/65, so the synth's own
    portamento does the sliding. Works with any synth, no bend-range setup.
- **Multi-Ch** — one MIDI channel per stroke so each line can bend
  independently (MPE-style synths).
- **Play / BPM** — an internal clock for the Standalone app, or for
  auditioning in a DAW that's stopped. As soon as the host transport
  plays, Sketchex follows the host.

### Wiring it up

Sketchex is a MIDI effect: it makes no sound of its own.

- **Ableton Live** — Live has no slot for third-party MIDI plugins, so
  Sketchex runs as an "instrument" on its own MIDI track and is routed to a
  synth on a second track: on the synth track set **MIDI From** to the
  Sketchex track, choose **Sketchex** (the plugin, not Pre/Post FX) in the
  second dropdown, and set **Monitor → In**.
- **Bitwig / Reaper / Cubase / Studio One** — insert before the instrument
  in the same chain; or route the track's MIDI output to another track.
- **Logic Pro** — load it as a **MIDI FX** (AU) slot above the instrument.
- **FL Studio** — load it as a generator, set its MIDI output port, and set
  the same input port on the synth.

## Building

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build
```

JUCE 8.0.15 is fetched automatically via `FetchContent`. The sequencer core
(`Source/Core`) is JUCE-free and unit-tested (`Tests/CoreTests.cpp`);
`Tests/ProcessorSmoke.cpp` drives the real plugin processor and canvas
headlessly. See `CMakeLists.txt` for the `SKETCHEX_VERSION` override used
by CI/the installer.

### Building the Windows installer locally

Requires [Inno Setup 6](https://jrsoftware.org/isinfo.php):

```
ISCC.exe Installer\Sketchex.iss
```

Produces `Installer\Output\SketchexSetup.exe`.

## Releasing a new version

Push a `v*` tag (e.g. `v1.0.0`) — `.github/workflows/release.yml` builds
Windows/macOS/Linux and publishes a GitHub Release with every installer
attached automatically. Use the workflow's manual "Run workflow" button to
test the build pipeline without publishing anything.

---

gexex © 2026
