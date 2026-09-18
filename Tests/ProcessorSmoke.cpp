// Headless end-to-end check: builds the real SketchexAudioProcessor,
// loads a sketch, drives processBlock with a fake host playhead for a
// few bars, and prints the MIDI it produced. No GUI, no audio device.

#include "PluginProcessor.h"
#include "Parameters.h"
#include "GUI/SketchCanvas.h"
#include "GUI/LookAndFeel.h"

#include <cstdio>

namespace
{
    struct FakePlayHead : public juce::AudioPlayHead
    {
        double ppq = 0.0;
        double bpm = 120.0;
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm(bpm);
            p.setPpqPosition(ppq);
            p.setIsPlaying(playing);
            p.setTimeSignature(TimeSignature { 4, 4 });
            return p;
        }
    };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    SketchexAudioProcessor proc;
    FakePlayHead ph;
    proc.setPlayHead(&ph);

    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayConfigDetails(0, 2, sr, block);
    proc.prepareToPlay(sr, block);

    // Rising line across the whole loop + a flat high line in the second half.
    auto& sk = proc.editableSketch();
    auto& a = sk.beginStroke(200, 0.9f);
    a.addPoint({ 0.0f, 0.0f });
    a.addPoint({ 1.0f, 1.0f });
    auto& b = sk.beginStroke(320, 0.6f);
    b.addPoint({ 0.5f, 0.8f });
    b.addPoint({ 1.0f, 0.8f });
    proc.publishSketch();

    // Glide on, bend mode, retrigger off, 1/8 grid.
    auto set = [&](const char* id, float v)
    {
        auto* p = proc.apvts.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    set(sketchex::param::rate, 1);
    set(sketchex::param::glide, 1.0f);
    set(sketchex::param::retrigger, 0.0f);

    juce::AudioBuffer<float> audio(2, block);
    juce::MidiBuffer midi;
    int noteOns = 0, noteOffs = 0, bends = 0, blocks = 0;
    int minNote = 127, maxNote = 0;
    const double spb = sr * 60.0 / ph.bpm;
    for (double ppq = 0.0; ppq < 8.0; ppq += block / spb)
    {
        ph.ppq = ppq;
        midi.clear();
        proc.processBlock(audio, midi);
        ++blocks;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                ++noteOns;
                minNote = juce::jmin(minNote, m.getNoteNumber());
                maxNote = juce::jmax(maxNote, m.getNoteNumber());
                if (noteOns <= 12)
                    std::printf("  ppq %.3f  on  %s vel %d ch %d\n", ppq + meta.samplePosition / spb,
                                juce::MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 4).toRawUTF8(),
                                m.getVelocity(), m.getChannel());
            }
            else if (m.isNoteOff()) ++noteOffs;
            else if (m.isPitchWheel()) ++bends;
        }
    }
    // Stop the host: everything must release.
    ph.playing = false;
    midi.clear();
    proc.processBlock(audio, midi);
    int offsOnStop = 0;
    for (const auto meta : midi) if (meta.getMessage().isNoteOff()) ++offsOnStop;

    std::printf("blocks=%d noteOns=%d noteOffs=%d bends=%d range=%d..%d offsOnStop=%d playheadX=%.3f\n",
                blocks, noteOns, noteOffs, bends, minNote, maxNote, offsOnStop, proc.getPlayheadX());

    bool ok = noteOns >= 8 && bends > 20 && offsOnStop >= 1 && minNote == 48 && maxNote >= 67;
    // State round trip keeps the sketch.
    juce::MemoryBlock state;
    proc.getStateInformation(state);
    SketchexAudioProcessor proc2;
    proc2.setStateInformation(state.getData(), (int) state.getSize());
    ok = ok && proc2.editableSketch().strokes.size() == 2;
    std::printf("state round-trip strokes=%d\n", (int) proc2.editableSketch().strokes.size());

    // --- Canvas editing through the real component (no window needed) ---
    {
        SketchexAudioProcessor p3;
        sketchex::SketchexLookAndFeel lnf;
        sketchex::SketchCanvas canvas(p3);
        canvas.setLookAndFeel(&lnf);
        canvas.setSize(800, 400);
        auto ev = [&](float x, float y, juce::ModifierKeys mods)
        {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), { x, y }, mods,
                                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &canvas, &canvas, juce::Time::getCurrentTime(),
                                    { x, y }, juce::Time::getCurrentTime(), 1, false);
        };
        const auto left = juce::ModifierKeys::leftButtonModifier;
        canvas.mouseDown(ev(50, 300, left));
        for (int i = 1; i <= 60; ++i)
            canvas.mouseDrag(ev(50.0f + i * 12.0f, 300.0f - 120.0f * std::sin(i / 8.0f), left));
        canvas.mouseUp(ev(770, 300, left));
        const int afterDraw = (int) p3.editableSketch().strokes.size();
        const int pts = afterDraw ? (int) p3.editableSketch().strokes[0].points.size() : 0;

        canvas.mouseDown(ev(400, p3.editableSketch().strokes.empty() ? 200.0f : 300.0f - 120.0f * std::sin(29.2f / 8.0f),
                            juce::ModifierKeys::rightButtonModifier));
        canvas.mouseUp(ev(400, 200, juce::ModifierKeys::rightButtonModifier));
        const int afterErase = (int) p3.editableSketch().strokes.size();
        canvas.undo();
        const int afterUndo = (int) p3.editableSketch().strokes.size();
        canvas.setLookAndFeel(nullptr);

        juce::Image img(juce::Image::ARGB, 800, 400, true);
        juce::Graphics g(img);
        canvas.paintEntireComponent(g, false);
        juce::File out = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("sketchex_canvas.png");
        juce::PNGImageFormat png;
        juce::FileOutputStream fos(out);
        if (fos.openedOk()) { fos.setPosition(0); fos.truncate(); png.writeImageToStream(img, fos); }

        std::printf("canvas: strokesAfterDraw=%d points=%d afterErase=%d afterUndo=%d png=%s\n",
                    afterDraw, pts, afterErase, afterUndo, out.getFullPathName().toRawUTF8());
        ok = ok && afterDraw == 1 && pts > 50 && afterErase == 2 && afterUndo == 1;
    }
    std::printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
