#pragma once

#include <JuceHeader.h>

namespace sketchex
{
    // Bright "candy glass" look: near-white sky gradient, frosted panels,
    // saturated accent hues, chunky rounded controls. Everything here is
    // vector-drawn, no image assets.
    class SketchexLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        SketchexLookAndFeel();

        static juce::Colour ink()        { return juce::Colour(0xFF1C2340); }
        static juce::Colour inkSoft()    { return juce::Colour(0xFF5B6486); }
        static juce::Colour panel()      { return juce::Colour(0xD9FFFFFF); }
        static juce::Colour panelEdge()  { return juce::Colour(0x331C2340); }
        static juce::Colour accent()     { return juce::Colour(0xFF3EC8FF); } // sky
        static juce::Colour accent2()    { return juce::Colour(0xFFFF5FA8); } // pink
        static juce::Colour accent3()    { return juce::Colour(0xFFFFC53E); } // amber
        static juce::Colour accent4()    { return juce::Colour(0xFF7CFF8E); } // mint

        static juce::Font titleFont(float h);
        static juce::Font uiFont(float h);

        void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                              float startAngle, float endAngle, juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                              juce::Slider::SliderStyle, juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& bg,
                                  bool highlighted, bool down) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                          juce::ComboBox&) override;
        void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        juce::Font getLabelFont(juce::Label&) override;
        juce::Font getTextButtonFont(juce::TextButton&, int h) override;
        void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
        juce::Label* createSliderTextBox(juce::Slider&) override;

        static const juce::Identifier accentProperty;
        static juce::Colour accentFor(juce::Component&);
    };
}
