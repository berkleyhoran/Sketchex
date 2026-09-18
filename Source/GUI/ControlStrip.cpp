#include "ControlStrip.h"

#include "../Parameters.h"
#include "LookAndFeel.h"

namespace sketchex
{

ControlStrip::ControlStrip(SketchexAudioProcessor& p, SketchCanvas& c) : processor(p), canvas(c)
{
    using L = SketchexLookAndFeel;

    makeCombo(root, param::root, param::rootNames(), "Root", L::accent());
    makeCombo(scale, param::scale, param::scaleNames(), "Scale", L::accent());
    makeKnob(octave, param::octave, "Octave", L::accent());
    makeKnob(range, param::range, "Range", L::accent());
    makeCombo(length, param::length, param::lengthNames(), "Length", L::accent3());
    makeCombo(rate, param::rate, param::rateNames(), "Rate", L::accent3());

    makeKnob(gate, param::gate, "Gate", L::accent3());
    makeKnob(glide, param::glide, "Glide", L::accent2());
    makeCombo(glideMode, param::glideMode, param::glideModeNames(), "Glide Mode", L::accent2());
    makeKnob(bendRange, param::bendRange, "Bend Rng", L::accent2());
    makeKnob(channel, param::channel, "Channel", L::accent4());
    makeKnob(velocity, param::velocity, "Velocity", L::accent4());
    makeKnob(hue, param::brushHue, "Colour", L::accent4());
    hue.slider.textFromValueFunction = [](double v) { return juce::String((int) v) + juce::String::charToString((juce::juce_wchar) 0xb0); };
    octave.slider.textFromValueFunction = [](double v) { return "C" + juce::String((int) v); };
    gate.slider.textFromValueFunction = [](double v) { return juce::String((int) std::round(v * 100.0)) + "%"; };
    glide.slider.textFromValueFunction = [](double v) { return juce::String((int) std::round(v * 100.0)) + "%"; };
    velocity.slider.textFromValueFunction = [](double v) { return juce::String((int) std::round(v * 127.0)); };
    bendRange.slider.textFromValueFunction = [](double v) { return juce::String((int) v) + " st"; };
    range.slider.textFromValueFunction = [](double v) { return juce::String((int) v) + " oct"; };
    for (auto* k : { &hue, &octave, &gate, &glide, &velocity, &bendRange, &range })
        k->slider.updateText();

    for (auto* b : { &retrigger, &multiChan })
    {
        b->setClickingTogglesState(true);
        addAndMakeVisible(b);
    }
    retrigger.getProperties().set(L::accentProperty, (juce::int64) L::accent3().getARGB());
    multiChan.getProperties().set(L::accentProperty, (juce::int64) L::accent4().getARGB());
    retriggerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, param::retrigger, retrigger);
    multiChanAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, param::multiChan, multiChan);
    retrigger.setTooltip("Re-fire the note on every grid step while the line is under the playhead");
    multiChan.setTooltip("One MIDI channel per stroke so each line can bend independently (MPE-style)");
    bendRange.slider.setTooltip("Must match the pitch-bend range of the synth after Sketchex");
    glideMode.box.setTooltip("Bend: pitch-bend follows your line exactly.  Legato: overlapping notes + CC5/65, so the synth's own portamento glides");

    for (auto* b : { &drawBtn, &eraseBtn, &undoBtn, &redoBtn, &clearBtn, &panicBtn })
        addAndMakeVisible(b);
    drawBtn.setClickingTogglesState(false);
    eraseBtn.setClickingTogglesState(false);
    drawBtn.getProperties().set(L::accentProperty, (juce::int64) L::accent().getARGB());
    eraseBtn.getProperties().set(L::accentProperty, (juce::int64) L::accent2().getARGB());
    clearBtn.getProperties().set(L::accentProperty, (juce::int64) L::accent2().getARGB());
    panicBtn.getProperties().set(L::accentProperty, (juce::int64) L::accent3().getARGB());
    drawBtn.onClick = [this] { canvas.setTool(SketchCanvas::Tool::draw); refresh(); };
    eraseBtn.onClick = [this] { canvas.setTool(SketchCanvas::Tool::erase); refresh(); };
    undoBtn.onClick = [this] { canvas.undo(); refresh(); };
    redoBtn.onClick = [this] { canvas.redo(); refresh(); };
    clearBtn.onClick = [this] { canvas.clearAll(); refresh(); };
    panicBtn.onClick = [this] { processor.panic(); };
    panicBtn.setTooltip("Send all-notes-off on every channel");
    refresh();
}

ControlStrip::~ControlStrip() = default;

void ControlStrip::makeKnob(Knob& k, const char* id, const juce::String& title, juce::Colour accent)
{
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    k.slider.getProperties().set(SketchexLookAndFeel::accentProperty, (juce::int64) accent.getARGB());
    k.slider.setScrollWheelEnabled(true);
    addAndMakeVisible(k.slider);
    k.label.setText(title, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setFont(SketchexLookAndFeel::titleFont(12.0f));
    k.label.setColour(juce::Label::textColourId, SketchexLookAndFeel::inkSoft());
    addAndMakeVisible(k.label);
    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id, k.slider);
}

void ControlStrip::makeCombo(Combo& c, const char* id, const juce::StringArray& items, const juce::String& title, juce::Colour accent)
{
    c.box.addItemList(items, 1);
    c.box.getProperties().set(SketchexLookAndFeel::accentProperty, (juce::int64) accent.getARGB());
    addAndMakeVisible(c.box);
    c.label.setText(title, juce::dontSendNotification);
    c.label.setJustificationType(juce::Justification::centred);
    c.label.setFont(SketchexLookAndFeel::titleFont(12.0f));
    c.label.setColour(juce::Label::textColourId, SketchexLookAndFeel::inkSoft());
    addAndMakeVisible(c.label);
    c.attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, id, c.box);
}

void ControlStrip::layoutLabelled(juce::Component& control, juce::Label& label, juce::Rectangle<int> cell)
{
    label.setBounds(cell.removeFromTop(16));
    control.setBounds(cell.reduced(2, 0));
}

void ControlStrip::refresh()
{
    const bool erase = canvas.getTool() == SketchCanvas::Tool::erase;
    drawBtn.setToggleState(! erase, juce::dontSendNotification);
    eraseBtn.setToggleState(erase, juce::dontSendNotification);
    undoBtn.setEnabled(canvas.canUndo());
    redoBtn.setEnabled(canvas.canRedo());
}

void ControlStrip::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(SketchexLookAndFeel::ink().withAlpha(0.08f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 14.0f);
    g.setColour(SketchexLookAndFeel::panel());
    g.fillRoundedRectangle(r, 14.0f);
    g.setColour(SketchexLookAndFeel::panelEdge());
    g.drawRoundedRectangle(r.reduced(0.5f), 14.0f, 1.0f);

    // Section headers
    g.setFont(SketchexLookAndFeel::titleFont(11.0f));
    auto header = [&](juce::Rectangle<int> area, const juce::String& text, juce::Colour c)
    {
        g.setColour(c.withAlpha(0.9f));
        g.drawText(text.toUpperCase(), area.removeFromTop(14), juce::Justification::centredLeft, false);
    };
    auto b = getLocalBounds().reduced(12, 8);
    const int toolsW = 96;
    auto tools = b.removeFromRight(toolsW);
    b.removeFromRight(10);
    auto top = b.removeFromTop(b.getHeight() / 2);
    header(top, "Scale & Time", SketchexLookAndFeel::accent());
    header(b, "Play & Glide", SketchexLookAndFeel::accent2());
    header(tools, "Tools", SketchexLookAndFeel::accent4());
}

void ControlStrip::resized()
{
    auto b = getLocalBounds().reduced(12, 8);
    const int toolsW = 96;
    auto tools = b.removeFromRight(toolsW);
    b.removeFromRight(10);

    // Tools column
    tools.removeFromTop(16);
    const int th = juce::jmax(22, (tools.getHeight() - 5 * 4) / 6);
    for (auto* btn : { &drawBtn, &eraseBtn, &undoBtn, &redoBtn, &clearBtn, &panicBtn })
    {
        btn->setBounds(tools.removeFromTop(th));
        tools.removeFromTop(4);
    }

    auto top = b.removeFromTop(b.getHeight() / 2);
    auto bottom = b;
    top.removeFromTop(14);
    bottom.removeFromTop(14);

    // Row 1: Root Scale Octave Range | Length Rate
    {
        const int comboW = 92, knobW = 84;
        auto r = top;
        layoutLabelled(root.box, root.label, r.removeFromLeft(72).withTrimmedBottom(r.getHeight() - 16 - 28));
        layoutLabelled(scale.box, scale.label, r.removeFromLeft(comboW + 30).withTrimmedBottom(r.getHeight() - 16 - 28));
        layoutLabelled(octave.slider, octave.label, r.removeFromLeft(knobW));
        layoutLabelled(range.slider, range.label, r.removeFromLeft(knobW));
        r.removeFromLeft(12);
        layoutLabelled(length.box, length.label, r.removeFromLeft(comboW).withTrimmedBottom(r.getHeight() - 16 - 28));
        layoutLabelled(rate.box, rate.label, r.removeFromLeft(72).withTrimmedBottom(r.getHeight() - 16 - 28));
        r.removeFromLeft(12);
        layoutLabelled(velocity.slider, velocity.label, r.removeFromLeft(knobW));
        layoutLabelled(hue.slider, hue.label, r.removeFromLeft(knobW));
    }
    // Row 2: Retrigger Gate | Glide GlideMode BendRange | MultiCh Channel
    {
        const int knobW = 84;
        auto r = bottom;
        auto retrigCell = r.removeFromLeft(88);
        retrigger.setBounds(retrigCell.withTrimmedTop(16).withHeight(28).reduced(2, 0));
        layoutLabelled(gate.slider, gate.label, r.removeFromLeft(knobW));
        r.removeFromLeft(12);
        layoutLabelled(glide.slider, glide.label, r.removeFromLeft(knobW));
        layoutLabelled(glideMode.box, glideMode.label, r.removeFromLeft(96).withTrimmedBottom(r.getHeight() - 16 - 28));
        layoutLabelled(bendRange.slider, bendRange.label, r.removeFromLeft(knobW));
        r.removeFromLeft(12);
        auto multiCell = r.removeFromLeft(88);
        multiChan.setBounds(multiCell.withTrimmedTop(16).withHeight(28).reduced(2, 0));
        layoutLabelled(channel.slider, channel.label, r.removeFromLeft(knobW));
    }
}

} // namespace sketchex
