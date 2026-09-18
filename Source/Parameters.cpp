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
    static const juce::StringArray names { "1 bar", "2 bars", "4 bars", "8 bars" };
    return names;
}

const juce::StringArray& rateNames()
{
    static const juce::StringArray names { "1/4", "1/8", "1/16", "1/32" };
    return names;
}

const juce::StringArray& glideModeNames()
{
    static const juce::StringArray names { "Bend", "Legato" };
    return names;
}

double lengthChoiceToBeats(int choice)
{
    static const double beats[] = { 4.0, 8.0, 16.0, 32.0 };
    return beats[juce::jlimit(0, 3, choice)];
}

int rateChoiceToStepsPerBeat(int choice)
{
    static const int steps[] = { 1, 2, 4, 8 };
    return steps[juce::jlimit(0, 3, choice)];
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { root, 1 }, "Root", rootNames(), 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { scale, 1 }, "Scale", scaleNames(), 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID { octave, 1 }, "Octave", 0, 7, 3));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID { range, 1 }, "Range", 1, 6, 2,
                                                    AudioParameterIntAttributes().withLabel("oct")));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { length, 1 }, "Length", lengthNames(), 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { rate, 1 }, "Rate", rateNames(), 2));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID { retrigger, 1 }, "Retrigger", true));
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
