#include "Scale.h"

#include <algorithm>
#include <cmath>

namespace sketchex
{

const std::vector<Scale>& allScales()
{
    static const std::vector<Scale> scales = {
        { "Major",          { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",          { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",         { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",       { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",         { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",     { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",        { 0, 1, 3, 5, 6, 8, 10 } },
        { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",  { 0, 2, 3, 5, 7, 9, 11 } },
        { "Major Pent",     { 0, 2, 4, 7, 9 } },
        { "Minor Pent",     { 0, 3, 5, 7, 10 } },
        { "Blues",          { 0, 3, 5, 6, 7, 10 } },
        { "Whole Tone",     { 0, 2, 4, 6, 8, 10 } },
        { "Chromatic",      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return scales;
}

void ScaleQuantizer::set(int rootNote, int scaleIndex, int lowOctave, int octaves)
{
    root = std::clamp(rootNote, 0, 11);
    scaleIdx = std::clamp(scaleIndex, 0, (int) allScales().size() - 1);
    lowOct = std::clamp(lowOctave, -1, 8);
    octs = std::clamp(octaves, 1, 8);
    laneCount = octs * (int) allScales()[(size_t) scaleIdx].degrees.size() + 1;
}

int ScaleQuantizer::laneForY(float y) const
{
    y = std::clamp(y, 0.0f, 1.0f);
    return std::clamp((int) std::lround(y * (float) (laneCount - 1)), 0, laneCount - 1);
}

int ScaleQuantizer::noteForLane(int lane) const
{
    lane = std::clamp(lane, 0, laneCount - 1);
    const auto& deg = allScales()[(size_t) scaleIdx].degrees;
    const int n = (int) deg.size();
    const int octave = lane / n;
    const int step = lane % n;
    // MIDI octave numbering: C4 = 60, so octave o's C is (o + 1) * 12.
    return std::clamp((lowOct + 1) * 12 + root + octave * 12 + deg[(size_t) step], 0, 127);
}

float ScaleQuantizer::continuousPitchForY(float y) const
{
    y = std::clamp(y, 0.0f, 1.0f);
    const float pos = y * (float) (laneCount - 1);
    const int lo = std::clamp((int) std::floor(pos), 0, laneCount - 1);
    const int hi = std::min(lo + 1, laneCount - 1);
    const float t = pos - (float) lo;
    return (1.0f - t) * (float) noteForLane(lo) + t * (float) noteForLane(hi);
}

float ScaleQuantizer::yForLane(int lane) const
{
    if (laneCount <= 1)
        return 0.0f;
    return (float) std::clamp(lane, 0, laneCount - 1) / (float) (laneCount - 1);
}

bool ScaleQuantizer::isRootLane(int lane) const
{
    const int n = (int) allScales()[(size_t) scaleIdx].degrees.size();
    return lane % n == 0;
}

std::string midiNoteName(int note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    note = std::clamp(note, 0, 127);
    return std::string(names[note % 12]) + std::to_string(note / 12 - 1);
}

} // namespace sketchex
