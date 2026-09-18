#pragma once

#include "Scale.h"
#include "Sketch.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sketchex
{

enum class GlideMode
{
    bend = 0,   // pitch-bend the held note to follow the drawn curve
    legato = 1  // overlapping note-on/off; the synth's own portamento glides
};

struct EngineSettings
{
    ScaleQuantizer quantizer;
    double loopBeats = 4.0;          // loop length in quarter notes
    int stepsPerBeat = 4;            // grid: 1 = 1/4, 2 = 1/8, 4 = 1/16, 8 = 1/32
    bool retrigger = true;           // re-fire the note at every grid step
    float gate = 1.0f;               // 0..1 fraction of a step the note is held (1 = tie into next)
    float glide = 0.0f;              // 0 = none, 1 = follow the curve exactly
    GlideMode glideMode = GlideMode::bend;
    int bendRangeSemis = 12;         // must match the downstream synth's bend range
    bool multiChannel = false;       // one MIDI channel per stroke (bend per voice)
    int baseChannel = 1;             // 1..16
};

struct BlockInfo
{
    bool playing = false;
    double ppqAtStart = 0.0;   // host position in quarter notes at sample 0
    double bpm = 120.0;
    double sampleRate = 44100.0;
    int numSamples = 0;
};

struct MidiEvent
{
    enum Type { noteOn, noteOff, pitchBend, controller };
    Type type;
    int sampleOffset;
    int channel;   // 1..16
    int data1;     // note / bend (0..16383) / cc number
    int data2;     // velocity / unused / cc value
};

// A step-trigger the UI can animate ("note popped here").
struct TriggerInfo
{
    uint32_t strokeId;
    int note;
    float x, y;
};

// Turns a Sketch + transport position into MIDI. Pure logic: no JUCE, no
// allocation on the hot path beyond the output vectors' capacity growth,
// deterministic given identical inputs (hence unit-testable).
class SequencerEngine
{
public:
    void reset();

    // Generates events for one audio block. `events` and `triggers` are
    // cleared first. Sample offsets are strictly within [0, numSamples).
    void process(const BlockInfo& block, const EngineSettings& settings, const Sketch& sketch,
                 std::vector<MidiEvent>& events, std::vector<TriggerInfo>& triggers);

    // Emits note-offs for everything sounding at the given offset.
    void allNotesOff(std::vector<MidiEvent>& events, int sampleOffset = 0);

    // Normalised loop position 0..1 at the last processed block start.
    float lastPlayheadX() const { return playheadX; }
    bool wasPlaying() const { return playing; }

    struct Voice
    {
        int note = -1;
        int lane = -1;
        int channel = 1;
        float bendSemis = 0.0f;      // currently sent bend (smoothed)
        int lastBend14 = 8192;
        double gateOffPpq = -1.0;    // pending gate note-off in host ppq, or -1
        bool active = false;
    };
    const std::unordered_map<uint32_t, Voice>& voices() const { return voiceMap; }

private:
    void handleStep(int sampleOffset, double ppq, const EngineSettings& s, const Sketch& sketch,
                    std::vector<MidiEvent>& events, std::vector<TriggerInfo>& triggers);
    void updateBends(int sampleOffset, double ppq, const EngineSettings& s, const Sketch& sketch,
                     std::vector<MidiEvent>& events, float smoothing);
    void noteOff(uint32_t strokeId, int sampleOffset, std::vector<MidiEvent>& events);
    int allocateChannel(const EngineSettings& s);

    std::unordered_map<uint32_t, Voice> voiceMap;
    std::vector<ActiveSample> scratch;
    float playheadX = 0.0f;
    bool playing = false;
    long lastStepIndex = -1;
    int channelRotor = 0;
    double lastPpq = -1.0;
    uint32_t leadStroke = 0;       // most recently triggered stroke (owns the bend wheel in single-channel mode)
    int lastPortamentoCc = -1;
};

} // namespace sketchex
