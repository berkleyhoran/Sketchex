#pragma once

#include <JuceHeader.h>

#include "Core/SequencerEngine.h"
#include "Core/Sketch.h"

#include <atomic>
#include <memory>

class SketchexAudioProcessor : public juce::AudioProcessor
{
public:
    SketchexAudioProcessor();
    ~SketchexAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ------------------------------------------------------------------
    // UI-facing API (message thread)
    // ------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    // The editable sketch lives on the message thread. After any edit,
    // call publishSketch() to hand a copy to the audio thread.
    sketchex::Sketch& editableSketch() { return uiSketch; }
    void publishSketch();

    // Internal transport: lets Standalone (and a DAW that's stopped) run
    // the sequencer from its own clock.
    void setInternalPlaying(bool shouldPlay);
    bool isInternalPlaying() const { return internalPlaying.load(); }
    void setInternalBpm(double bpm) { internalBpm.store(bpm); }
    double getInternalBpm() const { return internalBpm.load(); }
    bool isHostDrivingTransport() const { return hostPlaying.load(); }

    float getPlayheadX() const { return playheadX.load(); }
    bool isSequencerPlaying() const { return sequencerPlaying.load(); }
    double getCurrentBpm() const { return currentBpm.load(); }

    // Drains note-trigger events that happened since the last call (for
    // canvas animation). Returns the number written into `out`.
    int drainTriggers(std::vector<sketchex::TriggerInfo>& out);

    // The quantiser currently in use (for lane guides). Message thread
    // rebuilds it from the parameters; cheap.
    sketchex::ScaleQuantizer currentQuantizer() const;

    void panic() { panicRequested.store(true); }

private:
    sketchex::EngineSettings buildSettings() const;
    static juce::MidiMessage toJuce(const sketchex::MidiEvent& e);

    // Parameter handles (raw atomics from the APVTS).
    std::atomic<float>* pRoot = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pOctave = nullptr;
    std::atomic<float>* pRange = nullptr;
    std::atomic<float>* pLength = nullptr;
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pRetrigger = nullptr;
    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pGlide = nullptr;
    std::atomic<float>* pGlideMode = nullptr;
    std::atomic<float>* pBendRange = nullptr;
    std::atomic<float>* pMultiChan = nullptr;
    std::atomic<float>* pChannel = nullptr;

    // Sketch hand-off: UI allocates a copy into `pending` under the spin
    // lock; the audio thread swaps pointers (no allocation) and parks the
    // old one in `retired` for the UI to free on its next publish.
    sketchex::Sketch uiSketch;
    juce::SpinLock sketchLock;
    std::unique_ptr<sketchex::Sketch> pendingSketch;
    std::unique_ptr<sketchex::Sketch> retiredSketch;
    std::unique_ptr<sketchex::Sketch> audioSketch;

    sketchex::SequencerEngine engine;
    std::vector<sketchex::MidiEvent> eventScratch;
    std::vector<sketchex::TriggerInfo> triggerScratch;

    // Trigger FIFO audio -> UI.
    static constexpr int kTriggerFifoSize = 256;
    juce::AbstractFifo triggerFifo { kTriggerFifoSize };
    std::vector<sketchex::TriggerInfo> triggerStorage;

    std::atomic<bool> internalPlaying { false };
    std::atomic<double> internalBpm { 120.0 };
    double internalPpq = 0.0;
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> sequencerPlaying { false };
    std::atomic<float> playheadX { 0.0f };
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<bool> panicRequested { false };
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SketchexAudioProcessor)
};
