#include "Sketch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace sketchex
{

void Stroke::addPoint(Point p)
{
    p.x = std::clamp(p.x, 0.0f, 1.0f);
    p.y = std::clamp(p.y, 0.0f, 1.0f);
    points.push_back(p);
}

void Stroke::crossingsAt(float x, std::vector<float>& ys, float mergeEps) const
{
    ys.clear();
    if (points.empty())
        return;

    if (points.size() == 1)
    {
        if (std::abs(points.front().x - x) < 1e-6f)
            ys.push_back(points.front().y);
        return;
    }

    for (size_t i = 1; i < points.size(); ++i)
    {
        const auto& a = points[i - 1];
        const auto& b = points[i];
        // Half-open span so a vertex shared by two segments counts once.
        const bool fwd = a.x <= x && x < b.x;
        const bool back = b.x <= x && x < a.x;
        if (! fwd && ! back)
        {
            // Let the very last point (x == end) count for the final segment.
            if (i == points.size() - 1 && std::abs(b.x - x) < 1e-6f)
                ys.push_back(b.y);
            continue;
        }
        const float dx = b.x - a.x;
        const float t = std::abs(dx) > 1e-9f ? (x - a.x) / dx : 0.0f;
        ys.push_back(a.y + (b.y - a.y) * t);
    }

    std::sort(ys.begin(), ys.end());
    // Merge near-duplicates (tight turns, self-touching lines).
    size_t w = 0;
    for (size_t r = 0; r < ys.size(); ++r)
    {
        if (w > 0 && ys[r] - ys[w - 1] < mergeEps)
            ys[w - 1] = 0.5f * (ys[w - 1] + ys[r]);
        else
            ys[w++] = ys[r];
    }
    ys.resize(w);
}

bool Stroke::yAt(float x, float& yOut) const
{
    thread_local std::vector<float> ys;
    crossingsAt(x, ys);
    if (ys.empty())
        return false;
    yOut = ys.front();
    return true;
}

std::vector<Stroke> Stroke::erased(float x, float y, float radius, uint32_t& nextId) const
{
    std::vector<Stroke> out;
    Stroke current;
    const float r2 = radius * radius;
    bool first = true;

    auto flush = [&]()
    {
        if (current.points.size() >= 2)
        {
            current.hue = hue;
            current.velocity = velocity;
            current.id = first ? id : ++nextId;
            first = false;
            out.push_back(current);
        }
        current.points.clear();
    };

    for (const auto& p : points)
    {
        const float dx = p.x - x, dy = p.y - y;
        if (dx * dx + dy * dy <= r2)
            flush();
        else
            current.points.push_back(p);
    }
    flush();
    return out;
}

Stroke& Sketch::beginStroke(int hue, float velocity)
{
    Stroke s;
    s.id = nextId();
    s.hue = hue;
    s.velocity = velocity;
    strokes.push_back(std::move(s));
    return strokes.back();
}

void Sketch::erase(float x, float y, float radius)
{
    std::vector<Stroke> result;
    for (const auto& s : strokes)
    {
        auto pieces = s.erased(x, y, radius, idCounter);
        for (auto& p : pieces)
            result.push_back(std::move(p));
    }
    strokes = std::move(result);
}

void Sketch::sampleAt(float x, std::vector<ActiveSample>& out) const
{
    out.clear();
    thread_local std::vector<float> ys;
    for (const auto& s : strokes)
    {
        s.crossingsAt(x, ys);
        for (size_t i = 0; i < ys.size() && i < 256; ++i)
            out.push_back({ s.id, (int) i, ys[i], s.velocity });
    }
}

std::string Sketch::serialize() const
{
    std::ostringstream ss;
    for (const auto& s : strokes)
    {
        ss << s.id << ' ' << s.hue << ' ' << s.velocity;
        for (const auto& p : s.points)
            ss << ' ' << p.x << ',' << p.y;
        ss << '\n';
    }
    return ss.str();
}

Sketch Sketch::deserialize(const std::string& text)
{
    Sketch sk;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        std::istringstream ss(line);
        Stroke s;
        if (! (ss >> s.id >> s.hue >> s.velocity))
            continue;
        std::string tok;
        while (ss >> tok)
        {
            Point p;
            if (std::sscanf(tok.c_str(), "%f,%f", &p.x, &p.y) == 2)
                s.addPoint(p);
        }
        if (! s.points.empty())
        {
            sk.idCounter = std::max(sk.idCounter, s.id);
            sk.strokes.push_back(std::move(s));
        }
    }
    return sk;
}

} // namespace sketchex
