#include "SketchCanvas.h"

#include "../Parameters.h"
#include "LookAndFeel.h"

namespace sketchex
{

namespace
{
    constexpr int kFps = 60;
    constexpr float kEraserRadiusPx = 18.0f;

    juce::Colour hueColour(int hue, float sat = 0.85f, float bri = 1.0f)
    {
        return juce::Colour::fromHSV((float) hue / 360.0f, sat, bri, 1.0f);
    }
}

SketchCanvas::SketchCanvas(SketchexAudioProcessor& p) : processor(p)
{
    setOpaque(false);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    triggerScratch.reserve(64);
    particles.reserve(600);
    quantizer = processor.currentQuantizer();
    quantizerLanes = quantizer.numLanes();
    startTimerHz(kFps);
}

SketchCanvas::~SketchCanvas()
{
    stopTimer();
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
juce::Rectangle<float> SketchCanvas::plotArea() const
{
    return getLocalBounds().toFloat().reduced(10.0f).withTrimmedRight(16.0f);
}

juce::Rectangle<float> SketchCanvas::scrollbarArea() const
{
    auto a = getLocalBounds().toFloat().reduced(10.0f);
    return a.removeFromRight(12.0f);
}

float SketchCanvas::viewBottomOctaves() const
{
    const float h = viewHeightOctaves();
    const float o = processor.apvts.getRawParameterValue(param::octave)->load();
    return juce::jlimit(0.0f, (float) ScaleQuantizer::kOctaves - h, o);
}

float SketchCanvas::viewHeightOctaves() const
{
    return juce::jlimit(1.0f, (float) ScaleQuantizer::kOctaves,
                        processor.apvts.getRawParameterValue(param::range)->load());
}

void SketchCanvas::setViewBottomOctaves(float o)
{
    const float h = viewHeightOctaves();
    o = juce::jlimit(0.0f, (float) ScaleQuantizer::kOctaves - h, o);
    auto* p = processor.apvts.getParameter(param::octave);
    p->setValueNotifyingHost(p->convertTo0to1(o));
}

void SketchCanvas::setViewHeightOctaves(float h)
{
    h = juce::jlimit(1.0f, (float) ScaleQuantizer::kOctaves, std::round(h));
    auto* p = processor.apvts.getParameter(param::range);
    p->setValueNotifyingHost(p->convertTo0to1(h));
}

float SketchCanvas::yToPixel(float ny) const
{
    const auto a = plotArea();
    const float oct = ScaleQuantizer::yToOctaves(ny);
    const float rel = (oct - viewBottomOctaves()) / viewHeightOctaves(); // 0 = bottom of view, 1 = top
    return a.getBottom() - rel * a.getHeight();
}

juce::Point<float> SketchCanvas::toNorm(juce::Point<float> px) const
{
    const auto a = plotArea();
    const float rel = (a.getBottom() - px.y) / a.getHeight();
    const float oct = viewBottomOctaves() + rel * viewHeightOctaves();
    return { juce::jlimit(0.0f, 1.0f, (px.x - a.getX()) / a.getWidth()),
             juce::jlimit(0.0f, 1.0f, ScaleQuantizer::octavesToY(oct)) };
}

juce::Point<float> SketchCanvas::toPixel(float nx, float ny) const
{
    const auto a = plotArea();
    return { a.getX() + nx * a.getWidth(), yToPixel(ny) };
}

juce::Point<float> SketchCanvas::snapEdges(juce::Point<float> px) const
{
    // Hitting exactly x=0 (beat 1.1.1) or x=1 by hand is nearly
    // impossible, and a stroke that starts 3px in misses the first step
    // entirely. Snap to the edge when the pointer is anywhere near it.
    constexpr float kSnap = 14.0f;
    const auto a = plotArea();
    if (px.x < a.getX() + kSnap) px.x = a.getX();
    if (px.x > a.getRight() - kSnap) px.x = a.getRight();
    return px;
}

void SketchCanvas::resized()
{
    pathsDirty = true;
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------
void SketchCanvas::pushUndo()
{
    undoStack.push_back(processor.editableSketch().serialize());
    if (undoStack.size() > 64)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void SketchCanvas::commit()
{
    processor.publishSketch();
    pathsDirty = true;
    if (onSketchChanged)
        onSketchChanged();
    repaint();
}

void SketchCanvas::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back(processor.editableSketch().serialize());
    processor.editableSketch() = Sketch::deserialize(undoStack.back());
    undoStack.pop_back();
    commit();
}

void SketchCanvas::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back(processor.editableSketch().serialize());
    processor.editableSketch() = Sketch::deserialize(redoStack.back());
    redoStack.pop_back();
    commit();
}

void SketchCanvas::clearAll()
{
    if (processor.editableSketch().empty()) return;
    pushUndo();
    processor.editableSketch().clear();
    commit();
}

void SketchCanvas::mouseDown(const juce::MouseEvent& e)
{
    lastMouse = e.position;
    if (scrollbarArea().contains(e.position))
    {
        draggingScrollbar = true;
        const auto sb = scrollbarArea();
        const float h = viewHeightOctaves() / (float) ScaleQuantizer::kOctaves;
        const float top = 1.0f - (viewBottomOctaves() / (float) ScaleQuantizer::kOctaves + h);
        const float thumbY = sb.getY() + top * sb.getHeight();
        const float thumbH = h * sb.getHeight();
        scrollbarDragOffset = (e.position.y >= thumbY && e.position.y <= thumbY + thumbH)
                                  ? e.position.y - thumbY : thumbH * 0.5f;
        mouseDrag(e);
        return;
    }
    const auto n = toNorm(snapEdges(e.position));
    const bool eraseGesture = tool == Tool::erase || e.mods.isRightButtonDown() || e.mods.isAltDown();

    pushUndo();
    if (eraseGesture)
    {
        erasing = true;
        const auto a = plotArea();
        processor.editableSketch().erase(n.x, n.y, kEraserRadiusPx / a.getWidth());
        commit();
        return;
    }

    drawing = true;
    const int hue = (int) processor.apvts.getRawParameterValue(param::brushHue)->load();
    const float vel = processor.apvts.getRawParameterValue(param::velocity)->load();
    auto& s = processor.editableSketch().beginStroke(hue, vel);
    s.addPoint({ n.x, n.y });
    activeStrokeId = s.id;
    commit();
}

void SketchCanvas::mouseDrag(const juce::MouseEvent& e)
{
    lastMouse = e.position;
    if (draggingScrollbar)
    {
        const auto sb = scrollbarArea();
        const float h = viewHeightOctaves();
        const float thumbTopRel = (e.position.y - scrollbarDragOffset - sb.getY()) / sb.getHeight();
        const float bottomOct = (float) ScaleQuantizer::kOctaves * (1.0f - thumbTopRel) - h;
        setViewBottomOctaves(bottomOct);
        return;
    }
    const auto n = toNorm(snapEdges(e.position));
    if (erasing)
    {
        const auto a = plotArea();
        processor.editableSketch().erase(n.x, n.y, kEraserRadiusPx / a.getWidth());
        commit();
        return;
    }
    if (! drawing) return;

    for (auto& s : processor.editableSketch().strokes)
    {
        if (s.id == activeStrokeId)
        {
            // Skip micro-moves so paths stay light.
            const auto& last = s.points.back();
            const auto lp = toPixel(last.x, last.y);
            if (lp.getDistanceFrom(e.position) < 1.5f) return;
            s.addPoint({ n.x, n.y });
            break;
        }
    }
    commit();
}

void SketchCanvas::mouseUp(const juce::MouseEvent&)
{
    if (drawing)
    {
        // Drop degenerate single-click strokes.
        auto& strokes = processor.editableSketch().strokes;
        for (size_t i = 0; i < strokes.size(); ++i)
            if (strokes[i].id == activeStrokeId && strokes[i].points.size() < 2)
            {
                strokes.erase(strokes.begin() + (long) i);
                break;
            }
        commit();
    }
    drawing = false;
    erasing = false;
    draggingScrollbar = false;
    activeStrokeId = 0;
}

void SketchCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (drawing || erasing) return;
    const float delta = w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        // Zoom around the pointer's pitch.
        const float anchorOct = ScaleQuantizer::yToOctaves(toNorm(e.position).y);
        const float oldH = viewHeightOctaves();
        const float newH = juce::jlimit(1.0f, (float) ScaleQuantizer::kOctaves, oldH - (delta > 0 ? 1.0f : -1.0f));
        if (std::abs(newH - oldH) < 0.5f) return;
        const float frac = (anchorOct - viewBottomOctaves()) / oldH;
        setViewHeightOctaves(newH);
        setViewBottomOctaves(anchorOct - frac * newH);
    }
    else
    {
        setViewBottomOctaves(viewBottomOctaves() + delta * 1.5f);
    }
}

void SketchCanvas::mouseMove(const juce::MouseEvent& e)
{
    lastMouse = e.position;
    mouseInside = true;
}

void SketchCanvas::mouseExit(const juce::MouseEvent&)
{
    mouseInside = false;
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
void SketchCanvas::timerCallback()
{
    const float dt = 1.0f / (float) kFps;
    timeSeconds += dt;

    // Quantiser (for lanes) tracks the parameters.
    quantizer = processor.currentQuantizer();
    if (quantizer.numLanes() != quantizerLanes)
    {
        quantizerLanes = quantizer.numLanes();
        pathsDirty = true;
    }
    if (std::abs(viewBottomOctaves() - cachedViewBottom) > 1e-5f || std::abs(viewHeightOctaves() - cachedViewHeight) > 1e-5f)
    {
        cachedViewBottom = viewBottomOctaves();
        cachedViewHeight = viewHeightOctaves();
        pathsDirty = true;
    }

    // Playhead: ease toward the audio thread's value except on wrap.
    const float target = processor.getPlayheadX();
    if (std::abs(target - displayedPlayhead) > 0.5f)
        displayedPlayhead = target;
    else
        displayedPlayhead += (target - displayedPlayhead) * 0.6f;

    // Note triggers -> effects.
    triggerScratch.clear();
    processor.drainTriggers(triggerScratch);
    for (const auto& t : triggerScratch)
    {
        int hue = 190;
        for (const auto& s : processor.editableSketch().strokes)
            if (s.id == t.strokeId) { hue = s.hue; break; }
        const auto c = hueColour(hue);
        const auto pos = toPixel(t.x, t.y);
        const int n = 14 + rng.nextInt(10);
        for (int i = 0; i < n; ++i)
        {
            const float ang = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const float spd = 40.0f + rng.nextFloat() * 160.0f;
            Particle p;
            p.pos = pos;
            p.vel = { std::cos(ang) * spd, std::sin(ang) * spd - 30.0f };
            p.colour = c.withHue(c.getHue() + (rng.nextFloat() - 0.5f) * 0.08f);
            p.maxLife = p.life = 0.35f + rng.nextFloat() * 0.5f;
            p.size = 2.0f + rng.nextFloat() * 4.0f;
            particles.push_back(p);
        }
        ripples.push_back({ pos, c, 0.0f });
        flashes.push_back({ t.strokeId, quantizer.laneForY(t.y), 0.0f });
    }

    for (auto& p : particles)
    {
        p.life -= dt;
        p.vel.y += 220.0f * dt;
        p.vel *= 0.97f;
        p.pos += p.vel * dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                   [](const Particle& p) { return p.life <= 0.0f; }),
                    particles.end());
    for (auto& r : ripples) r.age += dt;
    ripples.erase(std::remove_if(ripples.begin(), ripples.end(), [](const Ripple& r) { return r.age > 0.6f; }),
                  ripples.end());
    for (auto& f : flashes) f.age += dt;
    flashes.erase(std::remove_if(flashes.begin(), flashes.end(), [](const NoteFlash& f) { return f.age > 0.25f; }),
                  flashes.end());

    repaint();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
void SketchCanvas::rebuildStrokePaths()
{
    cachedPaths.clear();
    for (const auto& s : processor.editableSketch().strokes)
    {
        if (s.points.size() < 2) continue;
        CachedStroke cs;
        cs.id = s.id;
        cs.colour = hueColour(s.hue);
        auto p0 = toPixel(s.points[0].x, s.points[0].y);
        cs.path.startNewSubPath(p0);
        for (size_t i = 1; i < s.points.size(); ++i)
        {
            const auto p = toPixel(s.points[i].x, s.points[i].y);
            cs.path.lineTo(p);
        }
        cachedPaths.push_back(std::move(cs));
    }
    pathsDirty = false;
}

void SketchCanvas::paintBackground(juce::Graphics& g, juce::Rectangle<float> a)
{
    // Frosted plot area with a subtle animated colour drift.
    const float drift = 0.5f + 0.5f * std::sin(timeSeconds * 0.25f);
    const auto top = juce::Colour(0xFFF7FBFF).interpolatedWith(juce::Colour(0xFFFFF5FB), drift * 0.4f);
    const auto bot = juce::Colour(0xFFEAF3FF).interpolatedWith(juce::Colour(0xFFEDF9F1), drift);
    g.setColour(SketchexLookAndFeel::ink().withAlpha(0.10f));
    g.fillRoundedRectangle(a.translated(0.0f, 3.0f).expanded(1.0f), 16.0f);
    g.setGradientFill(juce::ColourGradient(top, a.getX(), a.getY(), bot, a.getX(), a.getBottom(), false));
    g.fillRoundedRectangle(a, 16.0f);

    // Soft aurora blobs
    juce::Graphics::ScopedSaveState ss(g);
    juce::Path clip;
    clip.addRoundedRectangle(a, 16.0f);
    g.reduceClipRegion(clip);
    const juce::Colour blobs[] = { SketchexLookAndFeel::accent(), SketchexLookAndFeel::accent2(),
                                   SketchexLookAndFeel::accent4(), SketchexLookAndFeel::accent3() };
    for (int i = 0; i < 4; ++i)
    {
        const float ph = timeSeconds * (0.10f + 0.03f * (float) i) + (float) i * 1.7f;
        const float cx = a.getX() + a.getWidth() * (0.5f + 0.42f * std::sin(ph));
        const float cy = a.getY() + a.getHeight() * (0.5f + 0.38f * std::cos(ph * 0.8f + 1.0f));
        const float rad = a.getWidth() * 0.32f;
        g.setGradientFill(juce::ColourGradient(blobs[i].withAlpha(0.16f), cx, cy,
                                               blobs[i].withAlpha(0.0f), cx + rad, cy, true));
        g.fillEllipse(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
    }
}

void SketchCanvas::paintLanes(juce::Graphics& g, juce::Rectangle<float> a)
{
    const int lanes = quantizer.numLanes();
    if (lanes < 2) return;
    const float laneH = a.getHeight() / (viewHeightOctaves() * (float) quantizer.lanesPerOctave());

    for (int i = 0; i < lanes; ++i)
    {
        const float y = yToPixel(quantizer.yForLane(i));
        if (y < a.getY() - laneH || y > a.getBottom() + laneH) continue;
        const bool root = quantizer.isRootLane(i);
        float flash = 0.0f;
        for (const auto& f : flashes)
            if (f.lane == i) flash = juce::jmax(flash, 1.0f - f.age / 0.25f);

        if (root)
        {
            g.setColour(SketchexLookAndFeel::accent().withAlpha(0.10f + flash * 0.25f));
            g.fillRect(a.getX(), y - laneH * 0.5f, a.getWidth(), laneH);
        }
        else if (flash > 0.0f)
        {
            g.setColour(SketchexLookAndFeel::accent2().withAlpha(flash * 0.18f));
            g.fillRect(a.getX(), y - laneH * 0.5f, a.getWidth(), laneH);
        }
        g.setColour(SketchexLookAndFeel::ink().withAlpha(root ? 0.18f : 0.07f));
        g.drawHorizontalLine((int) y, a.getX() + 34.0f, a.getRight());

        if (laneH >= 11.0f)
        {
            g.setColour(SketchexLookAndFeel::inkSoft().withAlpha(root ? 1.0f : 0.6f));
            g.setFont(SketchexLookAndFeel::uiFont(juce::jmin(11.0f, laneH * 0.8f)).boldened());
            g.drawText(midiNoteName(quantizer.noteForLane(i)),
                       juce::Rectangle<float>(a.getX() + 4.0f, y - 7.0f, 28.0f, 14.0f),
                       juce::Justification::centredLeft, false);
        }
    }

    // Beat grid.
    const int lengthChoice = (int) processor.apvts.getRawParameterValue(param::length)->load();
    const int rateChoice = (int) processor.apvts.getRawParameterValue(param::rate)->load();
    const double beats = param::lengthChoiceToBeats(lengthChoice);
    const double stepsPerBeat = param::rateChoiceToStepsPerBeat(rateChoice);
    const float swing = processor.apvts.getRawParameterValue(param::swing)->load();
    const int totalSteps = (int) std::floor(beats * stepsPerBeat + 1e-6);
    // Beats/bars first (always), then the step grid if it isn't too dense.
    for (int b = 0; b <= (int) beats; ++b)
    {
        const float x = a.getX() + a.getWidth() * (float) b / (float) beats;
        const bool bar = (b % 4) == 0;
        g.setColour(SketchexLookAndFeel::ink().withAlpha(bar ? 0.22f : 0.11f));
        g.drawVerticalLine((int) x, a.getY(), a.getBottom());
    }
    if (totalSteps > 0 && a.getWidth() / (float) totalSteps >= 5.0f && stepsPerBeat > 1.0)
    {
        for (int i = 0; i <= totalSteps; ++i)
        {
            double stepBeats = (double) i / stepsPerBeat;
            if (i & 1) stepBeats += swing / (3.0 * stepsPerBeat);
            const float x = a.getX() + a.getWidth() * (float) (stepBeats / beats);
            g.setColour(SketchexLookAndFeel::ink().withAlpha(0.045f));
            g.drawVerticalLine((int) x, a.getY(), a.getBottom());
        }
    }
}

void SketchCanvas::paintStrokes(juce::Graphics& g, juce::Rectangle<float>)
{
    if (pathsDirty)
        rebuildStrokePaths();

    for (const auto& cs : cachedPaths)
    {
        float flash = 0.0f;
        for (const auto& f : flashes)
            if (f.strokeId == cs.id) flash = juce::jmax(flash, 1.0f - f.age / 0.25f);

        const bool live = cs.id == activeStrokeId;
        // Outer glow
        g.setColour(cs.colour.withAlpha(0.18f + flash * 0.25f));
        g.strokePath(cs.path, juce::PathStrokeType(14.0f + flash * 8.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour(cs.colour.withAlpha(0.35f + flash * 0.3f));
        g.strokePath(cs.path, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Core
        g.setColour(cs.colour.brighter(live ? 0.4f : 0.15f + flash * 0.6f));
        g.strokePath(cs.path, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Hot centre
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.strokePath(cs.path, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void SketchCanvas::paintPlayhead(juce::Graphics& g, juce::Rectangle<float> a)
{
    const float x = a.getX() + displayedPlayhead * a.getWidth();
    const bool playing = processor.isSequencerPlaying();
    const auto c = playing ? SketchexLookAndFeel::accent2() : SketchexLookAndFeel::inkSoft();

    // Trail
    if (playing)
    {
        const float trailW = juce::jmin(90.0f, a.getWidth() * 0.12f);
        g.setGradientFill(juce::ColourGradient(c.withAlpha(0.0f), x - trailW, 0.0f, c.withAlpha(0.22f), x, 0.0f, false));
        g.fillRect(juce::jmax(a.getX(), x - trailW), a.getY(), juce::jmin(trailW, x - a.getX()), a.getHeight());
    }
    // Glow + line
    g.setColour(c.withAlpha(0.35f));
    g.fillRect(x - 3.0f, a.getY(), 6.0f, a.getHeight());
    g.setColour(c);
    g.fillRect(x - 1.0f, a.getY(), 2.0f, a.getHeight());
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.fillRect(x - 0.5f, a.getY(), 1.0f, a.getHeight());

    // Cap
    juce::Path cap;
    cap.addTriangle(x - 7.0f, a.getY() - 1.0f, x + 7.0f, a.getY() - 1.0f, x, a.getY() + 9.0f);
    g.setColour(c);
    g.fillPath(cap);

    // Intersections: little glowing dots where the playhead crosses strokes.
    std::vector<ActiveSample> hits;
    processor.editableSketch().sampleAt(displayedPlayhead, hits);
    for (const auto& h : hits)
    {
        int hue = 190;
        for (const auto& s : processor.editableSketch().strokes)
            if (s.id == h.strokeId) { hue = s.hue; break; }
        const auto pos = toPixel(displayedPlayhead, h.y);
        const float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 12.0f);
        g.setColour(hueColour(hue).withAlpha(0.35f));
        g.fillEllipse(pos.x - 11.0f - pulse * 3.0f, pos.y - 11.0f - pulse * 3.0f, 22.0f + pulse * 6.0f, 22.0f + pulse * 6.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(pos.x - 5.0f, pos.y - 5.0f, 10.0f, 10.0f);
        g.setColour(hueColour(hue));
        g.drawEllipse(pos.x - 5.0f, pos.y - 5.0f, 10.0f, 10.0f, 2.0f);
    }
}

void SketchCanvas::paintParticles(juce::Graphics& g)
{
    for (const auto& r : ripples)
    {
        const float t = r.age / 0.6f;
        const float rad = 6.0f + t * 44.0f;
        g.setColour(r.colour.withAlpha((1.0f - t) * 0.6f));
        g.drawEllipse(r.pos.x - rad, r.pos.y - rad, rad * 2.0f, rad * 2.0f, 2.5f * (1.0f - t) + 0.5f);
    }
    for (const auto& p : particles)
    {
        const float t = p.life / p.maxLife;
        const float sz = p.size * (0.4f + 0.6f * t);
        g.setColour(p.colour.withAlpha(t * 0.9f));
        g.fillEllipse(p.pos.x - sz, p.pos.y - sz, sz * 2.0f, sz * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(t * 0.5f));
        g.fillEllipse(p.pos.x - sz * 0.4f, p.pos.y - sz * 0.4f, sz * 0.8f, sz * 0.8f);
    }
}

void SketchCanvas::paintScrollbar(juce::Graphics& g)
{
    const auto sb = scrollbarArea();
    g.setColour(SketchexLookAndFeel::ink().withAlpha(0.08f));
    g.fillRoundedRectangle(sb, 6.0f);
    const float h = viewHeightOctaves() / (float) ScaleQuantizer::kOctaves;
    const float top = 1.0f - (viewBottomOctaves() / (float) ScaleQuantizer::kOctaves + h);
    auto thumb = juce::Rectangle<float>(sb.getX(), sb.getY() + top * sb.getHeight(), sb.getWidth(), h * sb.getHeight());
    g.setColour(SketchexLookAndFeel::accent().withAlpha(draggingScrollbar ? 0.9f : 0.6f));
    g.fillRoundedRectangle(thumb.reduced(2.0f, 0.0f), 4.0f);
    // Octave ticks
    for (int o = 1; o < ScaleQuantizer::kOctaves; ++o)
    {
        const float y = sb.getBottom() - (float) o / (float) ScaleQuantizer::kOctaves * sb.getHeight();
        g.setColour(SketchexLookAndFeel::ink().withAlpha(0.18f));
        g.fillRect(sb.getX() + 3.0f, y - 0.5f, sb.getWidth() - 6.0f, 1.0f);
    }
}

void SketchCanvas::paintCursor(juce::Graphics& g)
{
    if (! mouseInside || drawing) return;
    if (tool == Tool::erase || erasing)
    {
        g.setColour(SketchexLookAndFeel::accent2().withAlpha(0.25f));
        g.fillEllipse(lastMouse.x - kEraserRadiusPx, lastMouse.y - kEraserRadiusPx, kEraserRadiusPx * 2.0f, kEraserRadiusPx * 2.0f);
        g.setColour(SketchexLookAndFeel::accent2());
        g.drawEllipse(lastMouse.x - kEraserRadiusPx, lastMouse.y - kEraserRadiusPx, kEraserRadiusPx * 2.0f, kEraserRadiusPx * 2.0f, 1.5f);
    }
    else
    {
        // Snap preview: which note would this land on?
        const auto n = toNorm(snapEdges(lastMouse));
        const int lane = quantizer.laneForY(n.y);
        const auto snap = toPixel(n.x, quantizer.yForLane(lane));
        const int hue = (int) processor.apvts.getRawParameterValue(param::brushHue)->load();
        g.setColour(hueColour(hue).withAlpha(0.5f));
        g.fillEllipse(snap.x - 4.0f, snap.y - 4.0f, 8.0f, 8.0f);
        g.setFont(SketchexLookAndFeel::uiFont(11.0f).boldened());
        g.setColour(SketchexLookAndFeel::ink().withAlpha(0.8f));
        g.drawText(midiNoteName(quantizer.noteForLane(lane)),
                   juce::Rectangle<float>(snap.x + 8.0f, snap.y - 16.0f, 40.0f, 14.0f), juce::Justification::centredLeft, false);
    }
}

void SketchCanvas::paint(juce::Graphics& g)
{
    const auto a = plotArea();
    paintBackground(g, a);
    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        clip.addRoundedRectangle(a, 16.0f);
        g.reduceClipRegion(clip);
        paintLanes(g, a);
        paintStrokes(g, a);
        paintPlayhead(g, a);
        paintParticles(g);
        paintCursor(g);
    }
    g.setColour(SketchexLookAndFeel::panelEdge());
    g.drawRoundedRectangle(a, 16.0f, 1.2f);
    paintScrollbar(g);

    if (processor.editableSketch().empty() && ! drawing)
    {
        g.setColour(SketchexLookAndFeel::inkSoft().withAlpha(0.7f));
        g.setFont(SketchexLookAndFeel::titleFont(22.0f));
        g.drawText("draw a melody", a, juce::Justification::centred, false);
        g.setFont(SketchexLookAndFeel::uiFont(13.0f));
        g.drawText("left-drag to draw anything  /  right-drag to erase  /  wheel scrolls, ctrl+wheel zooms octaves",
                   a.withTrimmedTop(a.getHeight() * 0.5f + 20.0f).withHeight(20.0f), juce::Justification::centred, false);
    }
}

} // namespace sketchex
