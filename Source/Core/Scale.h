#pragma once

#include <array>
#include <string>
#include <vector>

namespace sketchex
{

// A musical scale as a set of semitone offsets from the root within one
// octave, e.g. Major = {0,2,4,5,7,9,11}.
struct Scale
{
    const char* name;
    std::vector<int> degrees;
};

// The full list the user can pick from. Index into this with the plugin's
// "Scale" parameter -- order is part of the saved-state contract, so only
// ever append.
const std::vector<Scale>& allScales();

// Maps the sketch's vertical axis to scale-locked MIDI notes.
//
// The pitch space is always the full kOctaves octaves starting at C0
// (MIDI 12), ending on the root of the octave above the last full one.
// y is normalised 0..1 over that whole space (1 = top = highest lane), so
// a drawing keeps its absolute pitch no matter how the canvas is scrolled
// or zoomed -- the view is purely a GUI concern (see SketchCanvas).
class ScaleQuantizer
{
public:
    static constexpr int kOctaves = 8;   // C0 .. C8
    static constexpr int kLowestOctave = 0;

    void set(int rootNote /*0-11*/, int scaleIndex);

    int numLanes() const { return laneCount; }
    int lanesPerOctave() const { return perOctave; }

    // Nearest lane for a normalised y, and its MIDI note.
    int laneForY(float y) const;
    int noteForLane(int lane) const;
    int noteForY(float y) const { return noteForLane(laneForY(y)); }

    // Unquantised pitch in fractional MIDI note numbers, following the
    // drawn line exactly (used for "follow the curve" pitch-bend).
    float continuousPitchForY(float y) const;

    // Normalised y at the centre of a lane (for drawing lane guides).
    float yForLane(int lane) const;

    // Normalised y <-> position in octaves (0 .. kOctaves) -- what the
    // canvas view scrolls/zooms in.
    static float yToOctaves(float y) { return y * (float) kOctaves; }
    static float octavesToY(float o) { return o / (float) kOctaves; }

    bool isRootLane(int lane) const;

    int lowestNote() const { return noteForLane(0); }
    int highestNote() const { return noteForLane(laneCount - 1); }

private:
    int root = 0;
    int scaleIdx = 0;
    int perOctave = 7;
    int laneCount = 1;
};

std::string midiNoteName(int note);

} // namespace sketchex
