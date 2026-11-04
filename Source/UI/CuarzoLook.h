#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cuarzo::ui
{
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

namespace Colours
{
    inline const juce::Colour bg        { 0xff121419 };
    inline const juce::Colour panel     { 0xff1b1e25 };
    inline const juce::Colour panelEdge { 0xff2a2e38 };
    inline const juce::Colour well      { 0xff0e1014 };
    inline const juce::Colour text      { 0xffdde1e8 };
    inline const juce::Colour dim       { 0xff858b98 };

    inline const juce::Colour oscA   { 0xffff8a3d };
    inline const juce::Colour oscB   { 0xfff5c542 };
    inline const juce::Colour oscC   { 0xff3ddbc1 };
    inline const juce::Colour oscD   { 0xffa17dff };
    inline const juce::Colour filter { 0xff4aa8ff };
    inline const juce::Colour pitch  { 0xffff5c8a };
    inline const juce::Colour lfo    { 0xff86dc5a };
    inline const juce::Colour dist   { 0xffff5a4e };

    inline juce::Colour osc (int i)
    {
        switch (i) { case 0: return oscA; case 1: return oscB; case 2: return oscC; default: return oscD; }
    }
}

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

class CuarzoLook : public juce::LookAndFeel_V4
{
public:
    CuarzoLook()
    {
        using namespace juce;
        setColour (Slider::textBoxTextColourId,        Colours::text);
        setColour (Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId,   Colours::filter.withAlpha (0.4f));
        setColour (Label::textColourId,                Colours::dim);
        setColour (ComboBox::backgroundColourId,       Colours::well);
        setColour (ComboBox::outlineColourId,          Colours::panelEdge);
        setColour (ComboBox::textColourId,             Colours::text);
        setColour (ComboBox::arrowColourId,            Colours::dim);
        setColour (PopupMenu::backgroundColourId,      Colours::panel);
        setColour (PopupMenu::textColourId,            Colours::text);
        setColour (PopupMenu::highlightedBackgroundColourId, Colours::panelEdge);
        setColour (PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (TextButton::buttonColourId,         Colours::well);
        setColour (TextButton::buttonOnColourId,       Colours::panelEdge);
        setColour (TextButton::textColourOffId,        Colours::dim);
        setColour (TextButton::textColourOnId,         Colours::text);
        setColour (ToggleButton::textColourId,         Colours::dim);
        setColour (TextEditor::backgroundColourId,     Colours::well);
        setColour (TextEditor::textColourId,           Colours::text);
        setColour (TextEditor::highlightColourId,      Colours::filter.withAlpha (0.4f));
        setColour (CaretComponent::caretColourId,      Colours::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        const auto accent = s.findColour (Slider::rotarySliderFillColourId);
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float size = jmin (bounds.getWidth(), bounds.getHeight());
        bounds = bounds.withSizeKeepingCentre (size, size);
        const auto c = bounds.getCentre();
        const float r = size * 0.5f;
        const float track = jmax (2.5f, size * 0.075f);
        const float angle = startAngle + pos * (endAngle - startAngle);

        // Pista
        Path bgArc;
        bgArc.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::well);
        g.strokePath (bgArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

        // Valor (los parametros bipolares se dibujan desde el centro)
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        Path valArc;
        valArc.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (s.isEnabled() ? accent : accent.withSaturation (0.1f));
        g.strokePath (valArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

        // Cuerpo
        const float bodyR = r - track * 2.2f;
        g.setGradientFill (ColourGradient (Colour (0xff343946), c.x, c.y - bodyR, Colour (0xff1d2028), c.x, c.y + bodyR, false));
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (Colour (0xff3d4250));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        // Indicador
        Path p;
        p.addRoundedRectangle (-1.25f, -bodyR + 2.0f, 2.5f, bodyR * 0.55f, 1.2f);
        g.setColour (Colours::text);
        g.fillPath (p, AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.0f));
        l->setColour (juce::Label::textColourId, Colours::text.withAlpha (0.85f));
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineWhenEditingColourId, Colours::panelEdge);
        l->setColour (juce::Label::backgroundWhenEditingColourId, Colours::well);
        return l;
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override        { return font (12.5f); }
    juce::Font getPopupMenuFont() override                        { return font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (11.5f, true); }
    juce::Font getLabelFont (juce::Label& l) override             { return l.getFont(); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.withAlpha (0.28f) : Colours::well.brighter (over ? 0.08f : 0.0f).brighter (down ? 0.1f : 0.0f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? accent : Colours::panelEdge);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        auto pill = r.removeFromLeft (26.0f).withSizeKeepingCentre (24.0f, 13.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.withAlpha (0.85f) : Colours::well.brighter (over ? 0.1f : 0.0f));
        g.fillRoundedRectangle (pill, 6.5f);
        g.setColour (on ? accent : Colours::panelEdge);
        g.drawRoundedRectangle (pill, 6.5f, 1.0f);
        const float kx = on ? pill.getRight() - 11.5f : pill.getX() + 1.5f;
        g.setColour (on ? juce::Colours::white : Colours::dim);
        g.fillEllipse (kx, pill.getY() + 1.5f, 10.0f, 10.0f);

        g.setColour (on ? Colours::text : Colours::dim);
        g.setFont (font (12.0f));
        g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
    }
};
} // namespace cuarzo::ui
