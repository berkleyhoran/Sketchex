#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"

namespace sketchex
{
    // The drawing surface. Time runs left→right across the loop, pitch
    // bottom→top. Left-drag draws a stroke, right-drag (or Eraser tool)
    // erases, the playhead sweeps at host tempo and notes "pop" with a
    // particle burst as they fire.
    class SketchCanvas : public juce::Component,
                         private juce::Timer
    {
    public:
        enum class Tool { draw, erase };

        explicit SketchCanvas(SketchexAudioProcessor&);
        ~SketchCanvas() override;

        void setTool(Tool t) { tool = t; repaint(); }
        Tool getTool() const { return tool; }

        void undo();
        void redo();
        void clearAll();
        bool canUndo() const { return ! undoStack.empty(); }
        bool canRedo() const { return ! redoStack.empty(); }

        std::function<void()> onSketchChanged;

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;

    private:
        void timerCallback() override;

        juce::Rectangle<float> plotArea() const;
        juce::Point<float> toNorm(juce::Point<float> px) const;
        juce::Point<float> toPixel(float nx, float ny) const;

        void pushUndo();
        void commit();
        void rebuildStrokePaths();

        void paintBackground(juce::Graphics&, juce::Rectangle<float>);
        void paintLanes(juce::Graphics&, juce::Rectangle<float>);
        void paintStrokes(juce::Graphics&, juce::Rectangle<float>);
        void paintPlayhead(juce::Graphics&, juce::Rectangle<float>);
        void paintParticles(juce::Graphics&);
        void paintCursor(juce::Graphics&);

        SketchexAudioProcessor& processor;
        Tool tool = Tool::draw;
        bool drawing = false;
        bool erasing = false;
        uint32_t activeStrokeId = 0;
        juce::Point<float> lastMouse;
        bool mouseInside = false;

        std::vector<std::string> undoStack, redoStack;

        struct CachedStroke
        {
            uint32_t id;
            juce::Path path;
            juce::Colour colour;
        };
        std::vector<CachedStroke> cachedPaths;
        bool pathsDirty = true;

        struct Particle
        {
            juce::Point<float> pos, vel;
            juce::Colour colour;
            float life, maxLife, size;
        };
        std::vector<Particle> particles;

        struct Ripple
        {
            juce::Point<float> pos;
            juce::Colour colour;
            float age;
        };
        std::vector<Ripple> ripples;

        struct NoteFlash
        {
            uint32_t strokeId;
            int lane;
            float age;
        };
        std::vector<NoteFlash> flashes;

        std::vector<TriggerInfo> triggerScratch;
        float displayedPlayhead = 0.0f;
        float lastPlayheadForTrail = 0.0f;
        float timeSeconds = 0.0f;
        juce::Random rng;
        ScaleQuantizer quantizer;
        int quantizerLanes = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SketchCanvas)
    };
}
