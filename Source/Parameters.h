#pragma once

#include <JuceHeader.h>

namespace sketchex::param
{
    inline constexpr const char* root        = "root";        // choice 0..11
    inline constexpr const char* scale       = "scale";       // choice, index into allScales()
    inline constexpr const char* octave      = "octave";      // float, view position (bottom octave, 0..7)
    inline constexpr const char* range       = "range";       // int, visible octaves (1..8)
    inline constexpr const char* length      = "length";      // choice: bars 1/2 1 2 4 8 16
    inline constexpr const char* rate        = "rate";        // choice: see rateNames()
    inline constexpr const char* noteMode    = "noteMode";    // choice: Retrig / Hold
    inline constexpr const char* swing       = "swing";       // float 0..1
    inline constexpr const char* gate        = "gate";        // float 0.05..1
    inline constexpr const char* glide       = "glide";       // float 0..1
    inline constexpr const char* glideMode   = "glideMode";   // choice: Bend / Legato
    inline constexpr const char* bendRange   = "bendRange";   // int 1..24
    inline constexpr const char* multiChan   = "multiChan";   // bool
    inline constexpr const char* channel     = "channel";     // int 1..16
    inline constexpr const char* velocity    = "velocity";    // float 0..1 (for new strokes)
    inline constexpr const char* brushHue    = "brushHue";    // float 0..360 (cosmetic, new strokes)

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    const juce::StringArray& rootNames();
    const juce::StringArray& scaleNames();
    const juce::StringArray& lengthNames();
    const juce::StringArray& rateNames();
    const juce::StringArray& glideModeNames();
    const juce::StringArray& noteModeNames();

    double lengthChoiceToBeats(int choice);   // assumes 4/4 for "bars"
    double rateChoiceToStepsPerBeat(int choice);
}
