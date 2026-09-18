// Minimal self-contained test runner for the JUCE-free core. Run via
// `ctest` or directly: build/SketchexCoreTests.

#include "Core/Scale.h"
#include "Core/SequencerEngine.h"
#include "Core/Sketch.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sketchex;

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                                                   \
    do {                                                                                              \
        ++checks;                                                                                     \
        if (! (cond)) { ++failures; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); }   \
    } while (0)

static std::string str(const std::string& s) { return s; }
template <typename T> static std::string str(const T& v) { return std::to_string(v); }

#define CHECK_EQ(a, b)                                                                                    \
    do {                                                                                                  \
        ++checks;                                                                                         \
        auto _a = (a); auto _b = (b);                                                                     \
        if (! (_a == _b)) { ++failures; std::printf("  FAIL %s:%d: %s == %s (%s vs %s)\n", __FILE__,      \
            __LINE__, #a, #b, str(_a).c_str(), str(_b).c_str()); }                                        \
    } while (0)

// ----------------------------------------------------------------------------
static void testScale()
{
    std::printf("Scale\n");
    ScaleQuantizer q;
    q.set(0, 0, 4, 1); // C major, C4..C5
    CHECK_EQ(q.numLanes(), 8);
    CHECK_EQ(q.noteForLane(0), 60);
    CHECK_EQ(q.noteForLane(7), 72);
    CHECK_EQ(q.noteForLane(3), 65); // F4
    CHECK_EQ(q.noteForY(0.0f), 60);
    CHECK_EQ(q.noteForY(1.0f), 72);
    CHECK(q.isRootLane(0));
    CHECK(q.isRootLane(7));
    CHECK(! q.isRootLane(3));
    CHECK(std::fabs(q.continuousPitchForY(0.5f * (1.0f / 7.0f)) - 61.0f) < 1e-4f);

    q.set(9, 1, 3, 2); // A minor, A3..A5
    CHECK_EQ(q.numLanes(), 15);
    CHECK_EQ(q.noteForLane(0), 57);
    CHECK_EQ(q.noteForLane(14), 81);
    CHECK_EQ(q.noteForLane(2), 60); // C4

    q.set(0, 9, 4, 1); // major pent
    CHECK_EQ(q.numLanes(), 6);
    CHECK_EQ(midiNoteName(60), std::string("C4"));
    CHECK_EQ(midiNoteName(61), std::string("C#4"));
}

// ----------------------------------------------------------------------------
static void testSketch()
{
    std::printf("Sketch\n");
    Sketch sk;
    auto& s = sk.beginStroke(120, 0.9f);
    s.addPoint({ 0.2f, 0.0f });
    s.addPoint({ 0.4f, 1.0f });
    s.addPoint({ 0.3f, 0.5f }); // backwards -> clamped to x=0.4
    CHECK(std::fabs(s.points.back().x - 0.4f) < 1e-6f);

    float y = -1;
    CHECK(! s.yAt(0.1f, y));
    CHECK(s.yAt(0.3f, y));
    CHECK(std::fabs(y - 0.5f) < 1e-5f);
    CHECK(s.yAt(0.4f, y));
    CHECK(! s.yAt(0.41f, y));

    std::vector<ActiveSample> out;
    sk.sampleAt(0.3f, out);
    CHECK_EQ((int) out.size(), 1);
    CHECK(std::fabs(out[0].velocity - 0.9f) < 1e-6f);

    // Round trip.
    const auto text = sk.serialize();
    auto sk2 = Sketch::deserialize(text);
    CHECK_EQ((int) sk2.strokes.size(), 1);
    CHECK_EQ((int) sk2.strokes[0].points.size(), 3);
    CHECK_EQ(sk2.strokes[0].hue, 120);
    CHECK_EQ(sk2.strokes[0].id, s.id);
    CHECK(sk2.nextId() > s.id);

    // Erase the middle of a long line -> two pieces.
    Sketch sk3;
    auto& l = sk3.beginStroke(0, 1.0f);
    for (int i = 0; i <= 100; ++i)
        l.addPoint({ i / 100.0f, 0.5f });
    sk3.erase(0.5f, 0.5f, 0.05f);
    CHECK_EQ((int) sk3.strokes.size(), 2);
    CHECK(sk3.strokes[0].endX() < 0.46f);
    CHECK(sk3.strokes[1].startX() > 0.54f);
    CHECK(sk3.strokes[0].id != sk3.strokes[1].id);

    sk3.erase(0.2f, 0.5f, 1.0f);
    CHECK(sk3.empty());
}

// ----------------------------------------------------------------------------
static EngineSettings defaultSettings()
{
    EngineSettings s;
    s.quantizer.set(0, 0, 4, 1); // C major, C4..C5, 8 lanes
    s.loopBeats = 4.0;
    s.stepsPerBeat = 4;
    s.retrigger = true;
    s.gate = 1.0f;
    s.glide = 0.0f;
    return s;
}

static int count(const std::vector<MidiEvent>& ev, MidiEvent::Type t)
{
    int n = 0;
    for (auto& e : ev) n += e.type == t;
    return n;
}

// Runs the engine over `totalBeats` of transport in blocks and collects.
static void run(SequencerEngine& eng, const EngineSettings& s, const Sketch& sk, double totalBeats,
                std::vector<MidiEvent>& all, std::vector<TriggerInfo>& trig, int blockSize = 256,
                double startPpq = 0.0)
{
    BlockInfo b;
    b.playing = true;
    b.bpm = 120.0;
    b.sampleRate = 48000.0;
    b.numSamples = blockSize;
    const double spb = b.sampleRate * 60.0 / b.bpm;
    std::vector<MidiEvent> ev;
    std::vector<TriggerInfo> tr;
    const int numBlocks = (int) std::floor(totalBeats * spb / blockSize + 1e-6);
    for (int i = 0; i < numBlocks; ++i)
    {
        b.ppqAtStart = startPpq + i * blockSize / spb;
        eng.process(b, s, sk, ev, tr);
        for (auto& e : ev)
        {
            CHECK(e.sampleOffset >= 0 && e.sampleOffset < blockSize);
            all.push_back(e);
        }
        for (auto& t : tr) trig.push_back(t);
    }
}

static void testEngineBasics()
{
    std::printf("Engine: grid retrigger\n");
    auto s = defaultSettings();
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });  // whole loop, flat at the bottom lane = C4
    st.addPoint({ 1.0f, 0.0f });

    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 4.0, all, trig);

    // 4 beats * 4 steps = 16 triggers over one loop.
    CHECK_EQ((int) trig.size(), 16);
    CHECK_EQ(count(all, MidiEvent::noteOn), 16);
    CHECK_EQ(count(all, MidiEvent::noteOff), 15); // last note still held
    for (auto& e : all)
        if (e.type == MidiEvent::noteOn) CHECK_EQ(e.data1, 60);

    // Stop -> everything released.
    BlockInfo b;
    b.playing = false;
    b.numSamples = 256;
    std::vector<MidiEvent> ev;
    std::vector<TriggerInfo> tr;
    eng.process(b, s, sk, ev, tr);
    CHECK_EQ(count(ev, MidiEvent::noteOff), 1);
    CHECK(eng.voices().empty());
}

static void testEngineNoteOffOrdering()
{
    std::printf("Engine: note-off precedes retrigger at the same offset\n");
    auto s = defaultSettings();
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });
    st.addPoint({ 1.0f, 0.0f });
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 1.0, all, trig);
    // Sequence should be on, off, on, off, on ... never two ons back to back.
    MidiEvent::Type last = MidiEvent::noteOff;
    for (auto& e : all)
    {
        if (e.type != MidiEvent::noteOn && e.type != MidiEvent::noteOff) continue;
        CHECK(e.type != last);
        last = e.type;
    }
}

static void testEnginePitchFollowsCurve()
{
    std::printf("Engine: pitch follows y\n");
    auto s = defaultSettings();
    s.stepsPerBeat = 2; // 8 steps per 4-beat loop => 8 lanes, one per step
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });
    st.addPoint({ 1.0f, 1.0f }); // rising diagonal C4 -> C5
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 4.0, all, trig, 64);

    std::vector<int> notes;
    for (auto& e : all) if (e.type == MidiEvent::noteOn) notes.push_back(e.data1);
    CHECK_EQ((int) notes.size(), 8);
    for (size_t i = 1; i < notes.size(); ++i) CHECK(notes[i] >= notes[i - 1]);
    CHECK_EQ(notes.front(), 60);
    CHECK(notes.back() >= 71);
}

static void testEngineGap()
{
    std::printf("Engine: gaps release, stroke end releases\n");
    auto s = defaultSettings();
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.5f });
    st.addPoint({ 0.25f, 0.5f }); // covers first beat only
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 4.0, all, trig);
    // steps at x = 0, 1/16, 2/16, 3/16, 4/16(=0.25 inclusive) -> 5 triggers
    CHECK_EQ((int) trig.size(), 5);
    CHECK_EQ(count(all, MidiEvent::noteOn), 5);
    CHECK_EQ(count(all, MidiEvent::noteOff), 5); // last one released at the following step
    CHECK(eng.voices().empty());
}

static void testEngineGate()
{
    std::printf("Engine: gate shortens notes\n");
    auto s = defaultSettings();
    s.gate = 0.5f;
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });
    st.addPoint({ 1.0f, 0.0f });
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 1.0, all, trig, 64);
    CHECK_EQ(count(all, MidiEvent::noteOn), 4);
    CHECK_EQ(count(all, MidiEvent::noteOff), 4); // every note released by its own gate
}

static void testEngineLegatoHold()
{
    std::printf("Engine: retrigger off holds one note across lane-equal steps\n");
    auto s = defaultSettings();
    s.retrigger = false;
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });
    st.addPoint({ 1.0f, 0.0f });
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 4.0, all, trig);
    CHECK_EQ(count(all, MidiEvent::noteOn), 1);
    CHECK_EQ(count(all, MidiEvent::noteOff), 0);
}

static void testEngineLegatoGlide()
{
    std::printf("Engine: legato glide overlaps notes\n");
    auto s = defaultSettings();
    s.retrigger = false;
    s.glide = 1.0f;
    s.glideMode = GlideMode::legato;
    s.stepsPerBeat = 2;
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });
    st.addPoint({ 1.0f, 1.0f });
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 4.0, all, trig, 64);
    // 8 steps on a rising line, but two adjacent steps can quantise to the
    // same lane (no new note there) -- so: first note + one per lane change.
    const int ons = count(all, MidiEvent::noteOn);
    CHECK(ons >= 6 && ons <= 8);
    // Each change: noteOn(new) then noteOff(old) at the same offset.
    int overlaps = 0;
    for (size_t i = 1; i < all.size(); ++i)
        if (all[i - 1].type == MidiEvent::noteOn && all[i].type == MidiEvent::noteOff
            && all[i - 1].sampleOffset == all[i].sampleOffset && all[i - 1].data1 != all[i].data1)
            ++overlaps;
    CHECK_EQ(overlaps, ons - 1);
    CHECK(count(all, MidiEvent::controller) >= 2); // portamento CCs sent once
}

static void testEngineBendGlide()
{
    std::printf("Engine: bend glide emits pitch bend within range\n");
    auto s = defaultSettings();
    s.retrigger = false;
    s.glide = 1.0f;
    s.glideMode = GlideMode::bend;
    s.bendRangeSemis = 12;
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });
    st.addPoint({ 1.0f, 1.0f });
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 4.0, all, trig, 128);
    CHECK(count(all, MidiEvent::pitchBend) > 20);
    for (auto& e : all)
        if (e.type == MidiEvent::pitchBend) CHECK(e.data1 >= 0 && e.data1 <= 16383);
    // Bend values must be monotonic-ish upward between note changes (rising line).
    int rising = 0, falling = 0, lastB = -1;
    for (auto& e : all)
    {
        if (e.type == MidiEvent::noteOn) { lastB = -1; continue; }
        if (e.type != MidiEvent::pitchBend) continue;
        if (lastB >= 0) (e.data1 >= lastB ? rising : falling)++;
        lastB = e.data1;
    }
    CHECK(rising > falling * 3);
}

static void testEngineLoopWrap()
{
    std::printf("Engine: host loop jump re-syncs\n");
    auto s = defaultSettings();
    Sketch sk;
    auto& st = sk.beginStroke(0, 1.0f);
    st.addPoint({ 0.0f, 0.0f });
    st.addPoint({ 1.0f, 0.0f });
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 2.0, all, trig);      // ppq 0..2
    const size_t before = trig.size();
    run(eng, s, sk, 1.0, all, trig, 256, 0.0); // jump back to 0
    CHECK(trig.size() > before);
    CHECK(std::fabs(trig[before].x) < 0.01f); // first trigger after the jump is at loop start
}

static void testEngineChords()
{
    std::printf("Engine: overlapping strokes = chords\n");
    auto s = defaultSettings();
    Sketch sk;
    auto& a = sk.beginStroke(0, 1.0f);
    a.addPoint({ 0.0f, 0.0f }); a.addPoint({ 1.0f, 0.0f });
    auto& b = sk.beginStroke(0, 1.0f);
    b.addPoint({ 0.0f, 1.0f }); b.addPoint({ 1.0f, 1.0f });
    SequencerEngine eng;
    std::vector<MidiEvent> all;
    std::vector<TriggerInfo> trig;
    run(eng, s, sk, 1.0, all, trig);
    CHECK_EQ(count(all, MidiEvent::noteOn), 8);
    int c4 = 0, c5 = 0;
    for (auto& e : all) if (e.type == MidiEvent::noteOn) (e.data1 == 60 ? c4 : c5)++;
    CHECK_EQ(c4, 4);
    CHECK_EQ(c5, 4);
}

int main()
{
    testScale();
    testSketch();
    testEngineBasics();
    testEngineNoteOffOrdering();
    testEnginePitchFollowsCurve();
    testEngineGap();
    testEngineGate();
    testEngineLegatoHold();
    testEngineLegatoGlide();
    testEngineBendGlide();
    testEngineLoopWrap();
    testEngineChords();
    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
