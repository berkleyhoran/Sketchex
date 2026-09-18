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

// Maps the canvas's vertical axis to scale-locked MIDI notes.
//
// The playable range is `octaves` octaves starting at `lowOctave` (MIDI
// octave, where 4 => middle C = 60 for root C). y is normalised 0..1 with
// 1 = top of the canvas = highest note. The range always ends on the root
// one octave above the last full octave, so a 1-octave C major range is
// C..C (8 lanes), not C..B.
class ScaleQuantizer
{
public:
    void set(int rootNote /*0-11*/, int scaleIndex, int lowOctave, int octaves);

    int numLanes() const { return laneCount; }

    // Nearest lane for a normalised y, and its MIDI note.
    int laneForY(float y) const;
    int noteForLane(int lane) const;
    int noteForY(float y) const { return noteForLane(laneForY(y)); }

    // Unquantised pitch in fractional MIDI note numbers, following the
    // drawn line exactly (used for "follow the curve" pitch-bend).
    float continuousPitchForY(float y) const;

    // Normalised y at the centre of a lane (for drawing lane guides).
    float yForLane(int lane) const;

    bool isRootLane(int lane) const;

    int lowestNote() const { return noteForLane(0); }
    int highestNote() const { return noteForLane(laneCount - 1); }

private:
    int root = 0;
    int scaleIdx = 0;
    int lowOct = 3;
    int octs = 2;
    int laneCount = 1;
};

std::string midiNoteName(int note);

} // namespace sketchex
