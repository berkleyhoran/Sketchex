#include "LookAndFeel.h"

namespace sketchex
{

const juce::Identifier SketchexLookAndFeel::accentProperty("sketchexAccent");

juce::Colour SketchexLookAndFeel::accentFor(juce::Component& c)
{
    const auto& v = c.getProperties()[accentProperty];
    if (v.isInt() || v.isInt64())
        return juce::Colour((juce::uint32) (juce::int64) v);
    return accent();
}

juce::Font SketchexLookAndFeel::titleFont(float h)
{
    return juce::Font(juce::FontOptions(h, juce::Font::bold));
}

juce::Font SketchexLookAndFeel::uiFont(float h)
{
    return juce::Font(juce::FontOptions(h, juce::Font::plain));
}

SketchexLookAndFeel::SketchexLookAndFeel()
{
    setColour(juce::Label::textColourId, ink());
    setColour(juce::ComboBox::textColourId, ink());
    setColour(juce::ComboBox::backgroundColourId, panel());
    setColour(juce::ComboBox::outlineColourId, panelEdge());
    setColour(juce::ComboBox::arrowColourId, inkSoft());
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFFFFFFFF));
    setColour(juce::PopupMenu::textColourId, ink());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha(0.35f));
    setColour(juce::PopupMenu::highlightedTextColourId, ink());
    setColour(juce::TextButton::textColourOffId, ink());
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::ToggleButton::textColourId, ink());
    setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxTextColourId, ink());
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TooltipWindow::backgroundColourId, ink());
    setColour(juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour(juce::TooltipWindow::outlineColourId, juce::Colours::transparentBlack);
}

juce::Font SketchexLookAndFeel::getLabelFont(juce::Label& l)
{
    return uiFont(l.getFont().getHeight());
}

juce::Font SketchexLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return uiFont(14.0f).boldened();
}

juce::Font SketchexLookAndFeel::getTextButtonFont(juce::TextButton&, int h)
{
    return titleFont(juce::jmin(15.0f, (float) h * 0.55f));
}

void SketchexLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto accentC = accentFor(s);
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(4.0f);
    const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto circle = juce::Rectangle<float>(size, size).withCentre(bounds.getCentre());
    const float r = size * 0.5f;
    const auto c = circle.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float trackW = juce::jmax(3.0f, r * 0.16f);

    // Track
    juce::Path track;
    track.addCentredArc(c.x, c.y, r - trackW * 0.5f, r - trackW * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour(ink().withAlpha(0.10f));
    g.strokePath(track, juce::PathStrokeType(trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc with glow
    juce::Path arc;
    arc.addCentredArc(c.x, c.y, r - trackW * 0.5f, r - trackW * 0.5f, 0.0f, startAngle, angle, true);
    g.setColour(accentC.withAlpha(0.35f));
    g.strokePath(arc, juce::PathStrokeType(trackW * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accentC);
    g.strokePath(arc, juce::PathStrokeType(trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Glossy cap
    const auto cap = circle.reduced(trackW * 2.0f);
    g.setColour(ink().withAlpha(0.12f));
    g.fillEllipse(cap.translated(0.0f, 2.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, cap.getCentreX(), cap.getY(),
                                           juce::Colour(0xFFE7EEF9), cap.getCentreX(), cap.getBottom(), false));
    g.fillEllipse(cap);
    g.setColour(panelEdge());
    g.drawEllipse(cap, 1.0f);
    // Highlight
    auto hl = cap.reduced(cap.getWidth() * 0.18f).withHeight(cap.getHeight() * 0.42f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.9f), hl.getCentreX(), hl.getY(),
                                           juce::Colours::white.withAlpha(0.0f), hl.getCentreX(), hl.getBottom(), false));
    g.fillEllipse(hl);

    // Pointer
    const float pr = cap.getWidth() * 0.5f;
    juce::Point<float> tip(c.x + std::sin(angle) * pr * 0.82f, c.y - std::cos(angle) * pr * 0.82f);
    juce::Point<float> base(c.x + std::sin(angle) * pr * 0.35f, c.y - std::cos(angle) * pr * 0.35f);
    g.setColour(accentC.darker(0.15f));
    g.drawLine({ base, tip }, juce::jmax(2.5f, pr * 0.16f));
}

void SketchexLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto accentC = accentFor(s);
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        const float cy = (float) y + (float) h * 0.5f;
        const float th = 6.0f;
        auto track = juce::Rectangle<float>((float) x, cy - th * 0.5f, (float) w, th);
        g.setColour(ink().withAlpha(0.10f));
        g.fillRoundedRectangle(track, th * 0.5f);
        auto fill = track.withWidth(juce::jmax(th, pos - (float) x));
        g.setColour(accentC.withAlpha(0.35f));
        g.fillRoundedRectangle(fill.expanded(0.0f, 2.0f), th);
        g.setColour(accentC);
        g.fillRoundedRectangle(fill, th * 0.5f);
        const float kr = 9.0f;
        juce::Rectangle<float> knob(pos - kr, cy - kr, kr * 2.0f, kr * 2.0f);
        g.setColour(ink().withAlpha(0.15f));
        g.fillEllipse(knob.translated(0.0f, 1.5f));
        g.setColour(juce::Colours::white);
        g.fillEllipse(knob);
        g.setColour(accentC);
        g.drawEllipse(knob.reduced(1.0f), 2.0f);
    }
    else
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, 0, 0, style, s);
}

void SketchexLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool highlighted, bool down)
{
    const auto accentC = accentFor(b);
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const float radius = juce::jmin(10.0f, r.getHeight() * 0.35f);
    const bool on = b.getToggleState();

    g.setColour(ink().withAlpha(0.12f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.5f), radius);

    if (on)
    {
        g.setGradientFill(juce::ColourGradient(accentC.brighter(0.25f), r.getX(), r.getY(),
                                               accentC.darker(0.05f), r.getX(), r.getBottom(), false));
    }
    else
    {
        auto top = highlighted ? juce::Colours::white : juce::Colour(0xFFFCFDFF);
        auto bot = highlighted ? juce::Colour(0xFFEFF4FC) : juce::Colour(0xFFE6ECF7);
        g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
    }
    if (down) g.setColour(accentC.withAlpha(0.6f));
    g.fillRoundedRectangle(r, radius);

    // Gloss
    auto gloss = r.withHeight(r.getHeight() * 0.45f).reduced(2.0f, 1.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(on ? 0.45f : 0.8f), gloss.getX(), gloss.getY(),
                                           juce::Colours::white.withAlpha(0.0f), gloss.getX(), gloss.getBottom(), false));
    g.fillRoundedRectangle(gloss, radius * 0.8f);

    g.setColour(on ? accentC.darker(0.25f) : panelEdge());
    g.drawRoundedRectangle(r, radius, 1.0f);
}

void SketchexLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    drawButtonBackground(g, b, {}, highlighted, down);
    g.setColour(b.getToggleState() ? juce::Colours::white : ink());
    g.setFont(titleFont(juce::jmin(14.0f, (float) b.getHeight() * 0.5f)));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(6, 0), juce::Justification::centred, 1);
}

void SketchexLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(1.0f);
    const float radius = juce::jmin(10.0f, r.getHeight() * 0.35f);
    g.setColour(ink().withAlpha(0.12f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.5f), radius);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, r.getX(), r.getY(),
                                           juce::Colour(0xFFE9EFF9), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    g.setColour(box.hasKeyboardFocus(true) ? accentFor(box) : panelEdge());
    g.drawRoundedRectangle(r, radius, 1.0f);

    // Arrow
    auto arrowArea = r.removeFromRight(r.getHeight()).reduced(r.getHeight() * 0.3f);
    juce::Path p;
    p.addTriangle(arrowArea.getX(), arrowArea.getY() + arrowArea.getHeight() * 0.3f,
                  arrowArea.getRight(), arrowArea.getY() + arrowArea.getHeight() * 0.3f,
                  arrowArea.getCentreX(), arrowArea.getBottom() - arrowArea.getHeight() * 0.2f);
    g.setColour(accentFor(box));
    g.fillPath(p);
}

void SketchexLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - box.getHeight() - 8, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void SketchexLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h)
{
    auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h);
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(panelEdge());
    g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
}

juce::Label* SketchexLookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setFont(uiFont(12.0f).boldened());
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, ink());
    l->setColour(juce::Label::textWhenEditingColourId, ink());
    l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour(juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
    l->setColour(juce::Label::outlineWhenEditingColourId, accentFor(s));
    return l;
}

} // namespace sketchex
