#include "Parameters.h"

#include "Core/Scale.h"

namespace sketchex::param
{

const juce::StringArray& rootNames()
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}

const juce::StringArray& scaleNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n;
        for (const auto& s : allScales())
            n.add(s.name);
        return n;
    }();
    return names;
}

const juce::StringArray& lengthNames()
{
    static const juce::StringArray names { "1/2 bar", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" };
    return names;
}

const juce::StringArray& rateNames()
{
    static const juce::StringArray names { "1/1", "1/2", "1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D",
                                           "1/16", "1/16T", "1/16D", "1/32", "1/64" };
    return names;
}

const juce::StringArray& glideModeNames()
{
    static const juce::StringArray names { "Bend", "Legato" };
    return names;
}

const juce::StringArray& noteModeNames()
{
    static const juce::StringArray names { "Retrig", "Hold" };
    return names;
}

double lengthChoiceToBeats(int choice)
{
    static const double beats[] = { 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
    return beats[juce::jlimit(0, 5, choice)];
}

double rateChoiceToStepsPerBeat(int choice)
{
    // Steps per quarter note. T = triplet (x1.5), D = dotted (/1.5).
    static const double steps[] = { 0.25, 0.5, 1.0, 1.5, 1.0 / 1.5, 2.0, 3.0, 2.0 / 1.5,
                                    4.0, 6.0, 4.0 / 1.5, 8.0, 16.0 };
    return steps[juce::jlimit(0, 12, choice)];
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { root, 1 }, "Root", rootNames(), 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { scale, 1 }, "Scale", scaleNames(), 0));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID { octave, 1 }, "View Octave",
                                                      NormalisableRange<float>(0.0f, 7.0f, 0.01f), 3.0f));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID { range, 1 }, "View Range", 1, 8, 2,
                                                    AudioParameterIntAttributes().withLabel("oct")));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { length, 1 }, "Length", lengthNames(), 1));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { rate, 1 }, "Rate", rateNames(), 8));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { noteMode, 1 }, "Note Mode", noteModeNames(), 0));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID { swing, 1 }, "Swing",
                                                      NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID { gate, 1 }, "Gate",
                                                      NormalisableRange<float>(0.05f, 1.0f, 0.01f), 1.0f));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID { glide, 1 }, "Glide",
                                                      NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { glideMode, 1 }, "Glide Mode", glideModeNames(), 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID { bendRange, 1 }, "Bend Range", 1, 24, 12,
                                                    AudioParameterIntAttributes().withLabel("st")));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID { multiChan, 1 }, "Multi-Channel", false));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID { channel, 1 }, "MIDI Channel", 1, 16, 1));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID { velocity, 1 }, "Velocity",
                                                      NormalisableRange<float>(0.05f, 1.0f, 0.01f), 0.8f));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID { brushHue, 1 }, "Brush Hue",
                                                      NormalisableRange<float>(0.0f, 360.0f, 1.0f), 190.0f));

    return { p.begin(), p.end() };
}

} // namespace sketchex::param
