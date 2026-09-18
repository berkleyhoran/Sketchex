#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sketchex
{

struct Point
{
    float x = 0.0f; // 0..1 across the loop (time)
    float y = 0.0f; // 0..1, 1 = top of canvas = highest pitch
};

// One drawn line, completely free-form: it may go backwards, loop, or
// cross itself. At playback time the vertical playhead line at x can
// cross it any number of times, and every crossing is its own voice
// (a circle plays as two notes that spread apart and converge).
struct Stroke
{
    uint32_t id = 0;
    int hue = 0;            // 0..359, purely cosmetic
    float velocity = 0.8f;  // 0..1, MIDI velocity when this stroke fires
    std::vector<Point> points;

    void addPoint(Point p);

    float startX() const { return points.empty() ? 0.0f : points.front().x; }
    float endX() const { return points.empty() ? 0.0f : points.back().x; }

    // Every y where the vertical line at time x crosses this stroke,
    // sorted bottom-to-top. Crossings closer than mergeEps (e.g. the two
    // sides of a tight U-turn) are merged so they don't double-trigger.
    void crossingsAt(float x, std::vector<float>& ysOut, float mergeEps = 0.02f) const;

    // Convenience: first crossing, if any.
    bool yAt(float x, float& yOut) const;

    // Remove every point within `radius` of (x, y); may split the stroke
    // into several. Returns the surviving pieces (possibly empty).
    std::vector<Stroke> erased(float x, float y, float radius, uint32_t& nextId) const;
};

struct ActiveSample
{
    uint32_t strokeId;
    int crossing;      // index among this stroke's crossings at x (0 = lowest)
    float y;
    float velocity;

    // Stable per-voice key for the sequencer.
    uint64_t voiceKey() const { return ((uint64_t) strokeId << 8) | (uint64_t) (crossing & 0xff); }
};

class Sketch
{
public:
    std::vector<Stroke> strokes;

    uint32_t nextId() { return ++idCounter; }

    Stroke& beginStroke(int hue, float velocity);
    void erase(float x, float y, float radius);
    void clear() { strokes.clear(); }
    bool empty() const { return strokes.empty(); }

    // Every stroke covering time x.
    void sampleAt(float x, std::vector<ActiveSample>& out) const;

    // Compact text form for host state / presets. One stroke per line:
    //   id hue velocity x,y x,y x,y ...
    std::string serialize() const;
    static Sketch deserialize(const std::string& text);

private:
    uint32_t idCounter = 0;
};

} // namespace sketchex
