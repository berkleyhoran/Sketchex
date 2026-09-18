#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "SketchCanvas.h"

namespace sketchex
{
    // Two rows of chunky controls under the canvas: musical settings on
    // the left (root/scale/octave/range/length/rate), performance on the
    // right (retrigger/gate/glide/glide-mode/bend/channel), plus tools.
    class ControlStrip : public juce::Component
    {
    public:
        ControlStrip(SketchexAudioProcessor&, SketchCanvas&);
        ~ControlStrip() override;

        void resized() override;
        void paint(juce::Graphics&) override;

        // Called by the editor's timer so tool/undo buttons track state.
        void refresh();

    private:
        struct Knob
        {
            juce::Slider slider;
            juce::Label label;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
        };
        struct Combo
        {
            juce::ComboBox box;
            juce::Label label;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
        };

        void makeKnob(Knob&, const char* paramId, const juce::String& title, juce::Colour accent);
        void makeCombo(Combo&, const char* paramId, const juce::StringArray& items, const juce::String& title, juce::Colour accent);
        void layoutLabelled(juce::Component& control, juce::Label& label, juce::Rectangle<int> cell);

        SketchexAudioProcessor& processor;
        SketchCanvas& canvas;

        Combo root, scale, length, rate, glideMode, noteMode;
        Knob octave, range, gate, glide, bendRange, channel, velocity, hue, swing;
        juce::ToggleButton multiChan { "Multi-Ch" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> multiChanAttach;

        juce::TextButton drawBtn { "Draw" }, eraseBtn { "Erase" }, undoBtn { "Undo" }, redoBtn { "Redo" },
                         clearBtn { "Clear" }, panicBtn { "Panic" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlStrip)
    };
}
