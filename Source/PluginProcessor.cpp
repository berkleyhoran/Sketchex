#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"

using namespace sketchex;

SketchexAudioProcessor::SketchexAudioProcessor()
    : AudioProcessor(BusesProperties()
                         // A MIDI effect has no audio of its own, but some
                         // hosts refuse a plugin with zero buses -- a
                         // silent stereo out is the pragmatic middle ground.
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SketchexState", param::createLayout())
{
    pRoot = apvts.getRawParameterValue(param::root);
    pScale = apvts.getRawParameterValue(param::scale);
    pOctave = apvts.getRawParameterValue(param::octave);
    pRange = apvts.getRawParameterValue(param::range);
    pLength = apvts.getRawParameterValue(param::length);
    pRate = apvts.getRawParameterValue(param::rate);
    pNoteMode = apvts.getRawParameterValue(param::noteMode);
    pSwing = apvts.getRawParameterValue(param::swing);
    pGate = apvts.getRawParameterValue(param::gate);
    pGlide = apvts.getRawParameterValue(param::glide);
    pGlideMode = apvts.getRawParameterValue(param::glideMode);
    pBendRange = apvts.getRawParameterValue(param::bendRange);
    pMultiChan = apvts.getRawParameterValue(param::multiChan);
    pChannel = apvts.getRawParameterValue(param::channel);

    triggerStorage.resize((size_t) kTriggerFifoSize);
    audioSketch = std::make_unique<Sketch>();
    eventScratch.reserve(512);
    triggerScratch.reserve(64);

    // Standalone has no host transport, so start its own clock running --
    // the user just draws and hears it. Inside a DAW the host's transport
    // takes over whenever it's playing.
    if (juce::JUCEApplicationBase::isStandaloneApp())
        internalPlaying.store(true);
}

SketchexAudioProcessor::~SketchexAudioProcessor() = default;

void SketchexAudioProcessor::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    engine.reset();
}

void SketchexAudioProcessor::releaseResources() {}

bool SketchexAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::disabled();
}

EngineSettings SketchexAudioProcessor::buildSettings() const
{
    EngineSettings s;
    s.quantizer.set((int) pRoot->load(), (int) pScale->load());
    s.loopBeats = param::lengthChoiceToBeats((int) pLength->load());
    s.stepsPerBeat = param::rateChoiceToStepsPerBeat((int) pRate->load());
    s.swing = pSwing->load();
    s.retrigger = (int) pNoteMode->load() == 0;
    s.gate = pGate->load();
    s.glide = pGlide->load();
    s.glideMode = (int) pGlideMode->load() == 0 ? GlideMode::bend : GlideMode::legato;
    s.bendRangeSemis = (int) pBendRange->load();
    s.multiChannel = pMultiChan->load() > 0.5f;
    s.baseChannel = (int) pChannel->load();
    return s;
}

ScaleQuantizer SketchexAudioProcessor::currentQuantizer() const
{
    ScaleQuantizer q;
    q.set((int) pRoot->load(), (int) pScale->load());
    return q;
}

juce::MidiMessage SketchexAudioProcessor::toJuce(const MidiEvent& e)
{
    switch (e.type)
    {
        case MidiEvent::noteOn:     return juce::MidiMessage::noteOn(e.channel, e.data1, (juce::uint8) e.data2);
        case MidiEvent::noteOff:    return juce::MidiMessage::noteOff(e.channel, e.data1);
        case MidiEvent::pitchBend:  return juce::MidiMessage::pitchWheel(e.channel, e.data1);
        case MidiEvent::controller: return juce::MidiMessage::controllerEvent(e.channel, e.data1, e.data2);
    }
    return juce::MidiMessage();
}

void SketchexAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Pick up a freshly published sketch, if any (pointer swap only).
    {
        juce::SpinLock::ScopedTryLockType lock(sketchLock);
        if (lock.isLocked() && pendingSketch != nullptr)
        {
            retiredSketch = std::move(audioSketch);
            audioSketch = std::move(pendingSketch);
        }
    }

    BlockInfo block;
    block.sampleRate = sampleRate;
    block.numSamples = buffer.getNumSamples();

    bool hostIsPlaying = false;
    double bpm = internalBpm.load();
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying() && pos->getPpqPosition())
            {
                hostIsPlaying = true;
                block.ppqAtStart = *pos->getPpqPosition();
            }
        }
    }
    hostPlaying.store(hostIsPlaying);
    currentBpm.store(bpm);
    block.bpm = bpm;

    if (hostIsPlaying)
    {
        block.playing = true;
        internalPpq = block.ppqAtStart; // keep the free-run clock in sync for when the host stops
    }
    else if (internalPlaying.load())
    {
        block.playing = true;
        block.ppqAtStart = internalPpq;
        internalPpq += block.numSamples * bpm / (60.0 * sampleRate);
    }
    else
    {
        block.playing = false;
        block.ppqAtStart = internalPpq;
    }

    const auto settings = buildSettings();

    if (panicRequested.exchange(false))
    {
        engine.allNotesOff(eventScratch, 0);
        for (const auto& e : eventScratch)
            midi.addEvent(toJuce(e), e.sampleOffset);
        for (int ch = 1; ch <= 16; ++ch)
            midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        engine.reset();
    }

    engine.process(block, settings, *audioSketch, eventScratch, triggerScratch);

    for (const auto& e : eventScratch)
        midi.addEvent(toJuce(e), e.sampleOffset);

    // Publish triggers for the UI.
    if (! triggerScratch.empty())
    {
        int start1, size1, start2, size2;
        triggerFifo.prepareToWrite((int) triggerScratch.size(), start1, size1, start2, size2);
        int i = 0;
        for (int k = 0; k < size1; ++k) triggerStorage[(size_t) (start1 + k)] = triggerScratch[(size_t) i++];
        for (int k = 0; k < size2; ++k) triggerStorage[(size_t) (start2 + k)] = triggerScratch[(size_t) i++];
        triggerFifo.finishedWrite(size1 + size2);
    }

    sequencerPlaying.store(block.playing);
    // Playhead for the UI: position at the *end* of this block is closest
    // to what's audible when the frame is drawn.
    {
        const double ppqEnd = block.ppqAtStart + block.numSamples * bpm / (60.0 * sampleRate);
        double f = std::fmod(ppqEnd / settings.loopBeats, 1.0);
        if (f < 0.0) f += 1.0;
        playheadX.store((float) f);
    }
}

void SketchexAudioProcessor::publishSketch()
{
    auto copy = std::make_unique<Sketch>(uiSketch);
    std::unique_ptr<Sketch> toFree;
    {
        juce::SpinLock::ScopedLockType lock(sketchLock);
        toFree = std::move(retiredSketch);
        if (pendingSketch != nullptr)
            toFree = std::move(pendingSketch); // superseded before the audio thread took it
        pendingSketch = std::move(copy);
    }
    toFree.reset(); // freed outside the lock, on the message thread
}

void SketchexAudioProcessor::setInternalPlaying(bool shouldPlay)
{
    internalPlaying.store(shouldPlay);
}

int SketchexAudioProcessor::drainTriggers(std::vector<TriggerInfo>& out)
{
    const int ready = triggerFifo.getNumReady();
    if (ready <= 0)
        return 0;
    int start1, size1, start2, size2;
    triggerFifo.prepareToRead(ready, start1, size1, start2, size2);
    for (int k = 0; k < size1; ++k) out.push_back(triggerStorage[(size_t) (start1 + k)]);
    for (int k = 0; k < size2; ++k) out.push_back(triggerStorage[(size_t) (start2 + k)]);
    triggerFifo.finishedRead(size1 + size2);
    return size1 + size2;
}

void SketchexAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("sketch", juce::String(uiSketch.serialize()), nullptr);
    state.setProperty("coords", "global", nullptr); // y is absolute pitch (C0..C8), not view-relative
    state.setProperty("internalBpm", internalBpm.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SketchexAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            const juce::String sketchText = tree.getProperty("sketch", "").toString();
            if (tree.hasProperty("internalBpm"))
                internalBpm.store((double) tree.getProperty("internalBpm"));
            const bool globalCoords = tree.getProperty("coords", "").toString() == "global";
            // v0.1.x saved y relative to the visible octave window; convert
            // to absolute pitch space using the window that was saved with it.
            float oldOct = 3.0f, oldRange = 2.0f;
            if (! globalCoords)
            {
                if (auto c = tree.getChildWithProperty("id", "octave"); c.isValid()) oldOct = (float) c.getProperty("value");
                if (auto c = tree.getChildWithProperty("id", "range"); c.isValid()) oldRange = (float) c.getProperty("value");
            }
            tree.removeProperty("sketch", nullptr);
            tree.removeProperty("coords", nullptr);
            tree.removeProperty("internalBpm", nullptr);
            apvts.replaceState(tree);
            uiSketch = Sketch::deserialize(sketchText.toStdString());
            if (! globalCoords)
                for (auto& st : uiSketch.strokes)
                    for (auto& pt : st.points)
                        pt.y = ScaleQuantizer::octavesToY(oldOct + pt.y * oldRange);
            publishSketch();
        }
    }
}

juce::AudioProcessorEditor* SketchexAudioProcessor::createEditor()
{
    return new SketchexAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SketchexAudioProcessor();
}
