#include "SequencerEngine.h"

#include <algorithm>
#include <cmath>

namespace sketchex
{

namespace
{
    constexpr int kBendUpdateInterval = 64; // samples between pitch-bend refreshes
    constexpr float kBendSmoothMs = 8.0f;

    double loopX(double ppq, double loopBeats)
    {
        const double f = std::fmod(ppq / loopBeats, 1.0);
        return f < 0.0 ? f + 1.0 : f;
    }

    int bend14(float semis, int range)
    {
        const float n = std::clamp(semis / (float) std::max(1, range), -1.0f, 1.0f);
        return std::clamp(8192 + (int) std::lround(n * 8191.0f), 0, 16383);
    }
}

void SequencerEngine::reset()
{
    voiceMap.clear();
    playing = false;
    lastStepIndex = -1;
    channelRotor = 0;
    lastPpq = -1.0;
    leadVoice = 0;
}

int SequencerEngine::allocateChannel(const EngineSettings& s)
{
    if (! s.multiChannel)
        return std::clamp(s.baseChannel, 1, 16);
    // Rotate through the channels above the base one; wrap at 16.
    const int base = std::clamp(s.baseChannel, 1, 16);
    const int ch = ((base - 1 + channelRotor) % 16) + 1;
    channelRotor = (channelRotor + 1) % 16;
    return ch;
}

void SequencerEngine::noteOff(VoiceKey key, int sampleOffset, std::vector<MidiEvent>& events)
{
    auto it = voiceMap.find(key);
    if (it == voiceMap.end() || ! it->second.active)
        return;
    auto& v = it->second;
    events.push_back({ MidiEvent::noteOff, sampleOffset, v.channel, v.note, 0 });
    v.active = false;
    v.gateOffPpq = -1.0;
}

void SequencerEngine::allNotesOff(std::vector<MidiEvent>& events, int sampleOffset)
{
    for (auto& [id, v] : voiceMap)
        if (v.active)
        {
            events.push_back({ MidiEvent::noteOff, sampleOffset, v.channel, v.note, 0 });
            v.active = false;
        }
    voiceMap.clear();
}

void SequencerEngine::handleStep(int sampleOffset, double ppq, const EngineSettings& s, const Sketch& sketch,
                                 std::vector<MidiEvent>& events, std::vector<TriggerInfo>& triggers)
{
    const float x = (float) loopX(ppq, s.loopBeats);
    sketch.sampleAt(x, scratch);

    // Strokes that ended (or were erased) since the last step: release.
    for (auto it = voiceMap.begin(); it != voiceMap.end();)
    {
        const bool stillDrawn = std::any_of(scratch.begin(), scratch.end(),
                                            [&](const ActiveSample& a) { return a.voiceKey() == it->first; });
        if (! stillDrawn)
        {
            noteOff(it->first, sampleOffset, events);
            it = voiceMap.erase(it);
        }
        else
            ++it;
    }

    for (const auto& a : scratch)
    {
        const int lane = s.quantizer.laneForY(a.y);
        const int note = s.quantizer.noteForLane(lane);
        const int vel = std::clamp((int) std::lround(a.velocity * 127.0f), 1, 127);
        const VoiceKey key = a.voiceKey();
        auto& v = voiceMap[key];

        auto startNote = [&](bool resetBend)
        {
            if (! v.active)
                v.channel = allocateChannel(s);
            if (resetBend || ! v.active)
            {
                v.bendSemis = 0.0f;
                if (v.lastBend14 != 8192)
                {
                    events.push_back({ MidiEvent::pitchBend, sampleOffset, v.channel, 8192, 0 });
                    v.lastBend14 = 8192;
                }
            }
            events.push_back({ MidiEvent::noteOn, sampleOffset, v.channel, note, vel });
            v.note = note;
            v.lane = lane;
            v.active = true;
            v.gateOffPpq = s.gate < 0.999f ? ppq + (double) s.gate / (double) s.stepsPerBeat : -1.0;
            leadVoice = key;
            triggers.push_back({ a.strokeId, note, x, a.y });
        };

        if (! v.active)
        {
            startNote(true);
        }
        else if (s.retrigger)
        {
            noteOff(key, sampleOffset, events);
            startNote(true);
        }
        else if (lane != v.lane)
        {
            if (s.glideMode == GlideMode::legato && s.glide > 0.0f)
            {
                // Overlap: new note first, then release the old one, so a
                // mono synth with portamento slides between them.
                const int oldNote = v.note;
                const int ch = v.channel;
                events.push_back({ MidiEvent::noteOn, sampleOffset, ch, note, vel });
                events.push_back({ MidiEvent::noteOff, sampleOffset, ch, oldNote, 0 });
                v.note = note;
                v.lane = lane;
                v.gateOffPpq = s.gate < 0.999f ? ppq + (double) s.gate / (double) s.stepsPerBeat : -1.0;
                leadVoice = key;
                triggers.push_back({ a.strokeId, note, x, a.y });
            }
            else
            {
                // Keep the sounding pitch continuous across the note change:
                // the bend re-expresses the old pitch relative to the new note.
                const float soundingPitch = (float) v.note + v.bendSemis;
                noteOff(key, sampleOffset, events);
                v.active = false;
                v.channel = v.channel; // channel kept
                events.push_back({ MidiEvent::noteOn, sampleOffset, v.channel, note, vel });
                v.note = note;
                v.lane = lane;
                v.active = true;
                v.bendSemis = s.glide > 0.0f ? std::clamp(soundingPitch - (float) note,
                                                          -(float) s.bendRangeSemis, (float) s.bendRangeSemis)
                                             : 0.0f;
                const int b = bend14(v.bendSemis, s.bendRangeSemis);
                if (b != v.lastBend14)
                {
                    // Bend must land before the note-on to avoid a pitch blip.
                    events.insert(events.end() - 1, { MidiEvent::pitchBend, sampleOffset, v.channel, b, 0 });
                    v.lastBend14 = b;
                }
                v.gateOffPpq = s.gate < 0.999f ? ppq + (double) s.gate / (double) s.stepsPerBeat : -1.0;
                leadVoice = key;
                triggers.push_back({ a.strokeId, note, x, a.y });
            }
        }
    }
}

void SequencerEngine::updateBends(int sampleOffset, double ppq, const EngineSettings& s, const Sketch& sketch,
                                  std::vector<MidiEvent>& events, float smoothing)
{
    const float x = (float) loopX(ppq, s.loopBeats);
    sketch.sampleAt(x, scratch);
    for (auto& [key, v] : voiceMap)
    {
        if (! v.active)
            continue;
        const ActiveSample* hit = nullptr;
        for (const auto& a : scratch)
            if (a.voiceKey() == key) { hit = &a; break; }
        if (hit == nullptr)
            continue;

        const float target = s.glide * (s.quantizer.continuousPitchForY(hit->y) - (float) v.note);
        v.bendSemis += (target - v.bendSemis) * smoothing;

        // Single-channel mode: only the most recent voice drives the bend,
        // otherwise several voices would fight over one channel's wheel.
        if (! s.multiChannel && key != leadVoice)
            continue;

        const int b = bend14(v.bendSemis, s.bendRangeSemis);
        if (b != v.lastBend14)
        {
            events.push_back({ MidiEvent::pitchBend, sampleOffset, v.channel, b, 0 });
            v.lastBend14 = b;
        }
    }
}

void SequencerEngine::process(const BlockInfo& block, const EngineSettings& s, const Sketch& sketch,
                              std::vector<MidiEvent>& events, std::vector<TriggerInfo>& triggers)
{
    events.clear();
    triggers.clear();

    if (! block.playing)
    {
        if (playing)
            allNotesOff(events, 0);
        playing = false;
        lastPpq = -1.0;
        playheadX = (float) loopX(block.ppqAtStart, s.loopBeats);
        return;
    }

    const double samplesPerBeat = block.sampleRate * 60.0 / std::max(1.0, block.bpm);
    const double stepsPerBeat = (double) std::max(1, s.stepsPerBeat);
    const double stepAtStart = block.ppqAtStart * stepsPerBeat;
    const double ppqAtEnd = block.ppqAtStart + block.numSamples / samplesPerBeat;

    // Just started, or the host jumped/looped backwards: fire the current
    // step immediately rather than waiting for the next boundary.
    const bool jumped = lastPpq >= 0.0 && block.ppqAtStart < lastPpq - 1e-6;
    if (! playing || jumped)
    {
        allNotesOff(events, 0);
        lastStepIndex = (long) std::floor(stepAtStart + 1e-9) - 1;
        playing = true;
    }
    playheadX = (float) loopX(block.ppqAtStart, s.loopBeats);

    // Gate note-offs falling inside this block.
    for (auto& [id, v] : voiceMap)
    {
        if (v.active && v.gateOffPpq >= 0.0 && v.gateOffPpq < ppqAtEnd)
        {
            const int off = std::clamp((int) std::lround((v.gateOffPpq - block.ppqAtStart) * samplesPerBeat),
                                       0, block.numSamples - 1);
            events.push_back({ MidiEvent::noteOff, off, v.channel, v.note, 0 });
            v.active = false;
            v.gateOffPpq = -1.0;
        }
    }

    // Grid steps inside this block.
    for (;;)
    {
        const long next = lastStepIndex + 1;
        const double stepPpq = (double) next / stepsPerBeat;
        const double offsetD = (stepPpq - block.ppqAtStart) * samplesPerBeat;
        if (offsetD >= (double) block.numSamples - 0.5)
            break;
        const int offset = std::clamp((int) std::lround(offsetD), 0, block.numSamples - 1);
        handleStep(offset, stepPpq, s, sketch, events, triggers);
        lastStepIndex = next;
    }

    // Continuous pitch-bend following.
    if (s.glideMode == GlideMode::bend && s.glide > 0.0f)
    {
        const float dtMs = 1000.0f * (float) kBendUpdateInterval / (float) block.sampleRate;
        const float smoothing = 1.0f - std::exp(-dtMs / kBendSmoothMs);
        for (int i = 0; i < block.numSamples; i += kBendUpdateInterval)
            updateBends(i, block.ppqAtStart + i / samplesPerBeat, s, sketch, events, smoothing);
    }

    // Legato mode: tell the synth to use its portamento, scaled by glide.
    if (s.glideMode == GlideMode::legato)
    {
        const int ccVal = std::clamp((int) std::lround(s.glide * 127.0f), 0, 127);
        if (ccVal != lastPortamentoCc)
        {
            const int ch = std::clamp(s.baseChannel, 1, 16);
            events.push_back({ MidiEvent::controller, 0, ch, 65, ccVal > 0 ? 127 : 0 });
            events.push_back({ MidiEvent::controller, 0, ch, 5, ccVal });
            lastPortamentoCc = ccVal;
        }
    }
    else
        lastPortamentoCc = -1;

    lastPpq = ppqAtEnd;

    std::stable_sort(events.begin(), events.end(),
                     [](const MidiEvent& a, const MidiEvent& b) { return a.sampleOffset < b.sampleOffset; });
}

} // namespace sketchex
