#pragma once

#include "CuarzoLook.h"
#include "../DSP/Algorithms.h"
#include "../Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace cuarzo::ui
{
// Diagrama interactivo del algoritmo de enrutamiento (clic o rueda para cambiarlo)
class AlgorithmDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit AlgorithmDisplay (juce::AudioProcessorValueTreeState& s)
        : state (s),
          attachment (*s.getParameter (ID::algorithm), [this] (float v) { current = juce::jlimit (1, kNumAlgorithms, (int) std::round (v)); repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (U ("Clic izquierdo / derecho o rueda del ratón para cambiar de algoritmo"));
    }

    int getAlgorithm() const noexcept { return current; }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();

        // Cabecera con selector
        auto head = r.removeFromTop (24.0f);
        g.setColour (Colours::text);
        g.setFont (font (15.0f, true));
        g.drawText (String (current) + " / " + String (kNumAlgorithms), head, Justification::centred);
        g.setColour (Colours::dim);
        g.setFont (font (14.0f, true));
        g.drawText (U ("◀"), head.removeFromLeft (26.0f), Justification::centred);
        g.drawText (U ("▶"), head.removeFromRight (26.0f), Justification::centred);

        auto nameArea = r.removeFromBottom (18.0f);
        g.setFont (font (11.5f));
        g.setColour (Colours::dim);
        const auto& alg = algorithms()[(size_t) current - 1];
        g.drawFittedText (alg.name, nameArea.toNearestInt(), Justification::centred, 1);

        // Area del diagrama
        auto area = r.reduced (6.0f, 4.0f);
        g.setColour (Colours::well);
        g.fillRoundedRectangle (area, 6.0f);
        area = area.reduced (8.0f, 8.0f);

        const auto& layout = algorithmLayouts()[(size_t) current - 1];
        const float busY = area.getBottom() - 6.0f;
        const float boxW = 30.0f, boxH = 20.0f;
        float minY = 3.0f, maxY = 0.0f, minX = 3.0f, maxX = 0.0f;
        for (auto& op : layout) { minY = jmin (minY, op.y); maxY = jmax (maxY, op.y); minX = jmin (minX, op.x); maxX = jmax (maxX, op.x); }
        const float rows  = jmax (1.0f, maxY - minY);
        const float cols  = jmax (1.0f, maxX - minX);
        const float rowStep = jmin (44.0f, (area.getHeight() - 26.0f - boxH) / rows);
        const float colStep = jmin (44.0f, (area.getWidth() - boxW - 24.0f) / cols);
        const float gridTop = area.getY() + (area.getHeight() - 26.0f - boxH - rowStep * (maxY - minY)) * 0.5f;
        const float gridLeft = area.getCentreX() - 10.0f - colStep * (maxX - minX) * 0.5f;

        auto centreOf = [&] (int op)
        {
            const auto pos = layout[(size_t) op];
            return Point<float> (gridLeft + (pos.x - minX) * colStep, gridTop + boxH * 0.5f + (pos.y - minY) * rowStep);
        };

        // Conexiones de modulacion
        for (int dst = 0; dst < kNumOscs; ++dst)
            for (int src = 0; src < kNumOscs; ++src)
                if (alg.modulates (src, dst))
                {
                    auto a = centreOf (src).translated (0.0f, boxH * 0.5f);
                    auto b = centreOf (dst).translated (0.0f, -boxH * 0.5f);
                    Path p;
                    p.startNewSubPath (a);
                    p.cubicTo (a.x, (a.y + b.y) * 0.5f, b.x, (a.y + b.y) * 0.5f, b.x, b.y - 4.0f);
                    g.setColour (Colours::osc (src).withAlpha (0.85f));
                    g.strokePath (p, PathStrokeType (1.6f));
                    Path tip;
                    tip.addTriangle (b.x - 4.0f, b.y - 6.0f, b.x + 4.0f, b.y - 6.0f, b.x, b.y);
                    g.fillPath (tip);
                }

        // Bus de salida
        float busL = 1.0e9f, busR = -1.0e9f;
        for (int i = 0; i < kNumOscs; ++i)
            if (alg.isCarrier (i))
            {
                auto c = centreOf (i);
                busL = jmin (busL, c.x); busR = jmax (busR, c.x);
                g.setColour (Colours::text.withAlpha (0.5f));
                g.drawLine (c.x, c.y + boxH * 0.5f, c.x, busY, 1.5f);
            }
        g.setColour (Colours::text.withAlpha (0.5f));
        g.drawLine (busL - 6.0f, busY, busR + 6.0f, busY, 1.5f);
        g.setFont (font (9.5f, true));
        g.drawText ("OUT", Rectangle<float> (busR + 8.0f, busY - 7.0f, 30.0f, 14.0f), Justification::centredLeft);

        // Osciladores
        for (int i = 0; i < kNumOscs; ++i)
        {
            const auto c = centreOf (i);
            auto box = Rectangle<float> (boxW, boxH).withCentre (c);
            const float level = state.getRawParameterValue (ID::osc (i, "level"))->load();
            const float fb    = state.getRawParameterValue (ID::osc (i, "fb"))->load();
            const auto col = Colours::osc (i).withMultipliedAlpha (level > 0.001f ? 1.0f : 0.35f);

            g.setColour (col.withAlpha (col.getFloatAlpha() * 0.22f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (col);
            g.drawRoundedRectangle (box, 4.0f, alg.isCarrier (i) ? 2.0f : 1.2f);
            g.setFont (font (13.0f, true));
            g.drawText (oscLetter (i), box, Justification::centred);

            if (fb > 0.001f) // indicador de feedback
            {
                Path loop;
                loop.addCentredArc (box.getRight() + 1.0f, box.getY() + 4.0f, 6.0f, 6.0f, 0.0f, -2.0f, 2.6f, true);
                g.strokePath (loop, PathStrokeType (1.2f));
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool back = e.mods.isRightButtonDown() || e.x < getWidth() / 3;
        step (back ? -1 : 1);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (std::abs (w.deltaY) > 0.0f) step (w.deltaY > 0 ? -1 : 1);
    }

private:
    void step (int delta)
    {
        int next = current + delta;
        if (next < 1) next = kNumAlgorithms;
        if (next > kNumAlgorithms) next = 1;
        attachment.setValueAsCompleteGesture ((float) next);
    }

    juce::AudioProcessorValueTreeState& state;
    juce::ParameterAttachment attachment;
    int current = 1;
};
} // namespace cuarzo::ui
