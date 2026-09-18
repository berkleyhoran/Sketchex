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
    if (! points.empty())
        p.x = std::max(p.x, points.back().x);
    points.push_back(p);
}

bool Stroke::yAt(float x, float& yOut) const
{
    if (points.empty() || x < points.front().x || x > points.back().x)
        return false;

    if (points.size() == 1)
    {
        yOut = points.front().y;
        return true;
    }

    // First segment whose end is at/after x. Points are sorted by x so a
    // binary search would work, but strokes are short (hundreds of points)
    // and this runs a handful of times per audio block.
    for (size_t i = 1; i < points.size(); ++i)
    {
        const auto& a = points[i - 1];
        const auto& b = points[i];
        if (x <= b.x)
        {
            const float dx = b.x - a.x;
            const float t = dx > 1e-6f ? (x - a.x) / dx : 1.0f;
            yOut = a.y + (b.y - a.y) * t;
            return true;
        }
    }
    yOut = points.back().y;
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
    for (const auto& s : strokes)
    {
        float y;
        if (s.yAt(x, y))
            out.push_back({ s.id, y, s.velocity });
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
