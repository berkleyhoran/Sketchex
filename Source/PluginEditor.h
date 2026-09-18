#pragma once

#include <JuceHeader.h>

#include "GUI/ControlStrip.h"
#include "GUI/LookAndFeel.h"
#include "GUI/SketchCanvas.h"
#include "PluginProcessor.h"
#include "UpdateChecker.h"

class SketchexAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit SketchexAudioProcessorEditor(SketchexAudioProcessor&);
    ~SketchexAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void checkForUpdates();
    void updateTransportButton();

    SketchexAudioProcessor& processor;
    sketchex::SketchexLookAndFeel lookAndFeel;
    sketchex::SketchCanvas canvas;
    sketchex::ControlStrip controls;

    juce::TextButton playButton { "Play" };
    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    juce::TextButton checkUpdatesButton { "Check for Updates" };
    UpdateChecker updateChecker;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SketchexAudioProcessorEditor)
};
