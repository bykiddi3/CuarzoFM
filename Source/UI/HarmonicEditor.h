#pragma once

#include "CuarzoLook.h"
#include "../PluginProcessor.h"

namespace cuarzo::ui
{
// Editor de forma de onda por dibujo de armonicos (64 parciales por oscilador)
class HarmonicEditor : public juce::Component
{
public:
    using Harmonics = CuarzoProcessor::Harmonics;

    HarmonicEditor (CuarzoProcessor& p) : proc (p)
    {
        for (int i = 0; i < kNumOscs; ++i)
        {
            auto& t = tabs[(size_t) i];
            t.setButtonText ("OSC " + oscLetter (i));
            t.setClickingTogglesState (false);
            t.setColour (juce::TextButton::buttonOnColourId, Colours::osc (i));
            t.onClick = [this, i] { selectOsc (i); };
            addAndMakeVisible (t);
        }

        const juce::StringArray names { U ("Seno"), U ("Sierra"), U ("Cuadrada"), U ("Órgano"), U ("Aleatoria"), U ("Limpiar") };
        for (int i = 0; i < (int) shapes.size(); ++i)
        {
            auto& b = shapes[(size_t) i];
            b.setButtonText (names[i]);
            b.onClick = [this, i] { applyShape (i); };
            addAndMakeVisible (b);
        }

        selectOsc (0);
    }

    std::function<void (int osc)> onUserEdit; // p.ej. cambiar la forma del oscilador a "Dibujada"

    void selectOsc (int i)
    {
        pushIfDirty();
        selected = i;
        for (int t = 0; t < kNumOscs; ++t)
            tabs[(size_t) t].setToggleState (t == i, juce::dontSendNotification);
        refreshFromProcessor();
    }

    void refreshFromProcessor()
    {
        if (dragging) return;
        values = proc.getHarmonics (selected);
        repaint();
    }

    void pushIfDirty()
    {
        if (! dirty) return;
        dirty = false;
        proc.setHarmonics (selected, values);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (24);
        for (int i = kNumOscs - 1; i >= 0; --i)
        {
            tabs[(size_t) i].setBounds (top.removeFromRight (54).reduced (2, 1));
        }
        auto bottom = r.removeFromBottom (26);
        const int bw = bottom.getWidth() / (int) shapes.size();
        for (auto& b : shapes)
            b.setBounds (bottom.removeFromLeft (bw).reduced (2, 2));
        barsArea = r.reduced (0, 6);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto accent = Colours::osc (selected);
        auto area = barsArea.toFloat();

        g.setColour (Colours::well);
        g.fillRoundedRectangle (area, 5.0f);

        auto inner = area.reduced (6.0f, 6.0f);
        const float bw = inner.getWidth() / (float) kNumHarmonics;

        // Lineas guia
        g.setColour (Colours::panelEdge.withAlpha (0.6f));
        for (int q = 1; q < 4; ++q)
            g.drawHorizontalLine ((int) (inner.getY() + inner.getHeight() * q / 4.0f), inner.getX(), inner.getRight());

        for (int n = 0; n < kNumHarmonics; ++n)
        {
            const float v = values[(size_t) n];
            auto bar = Rectangle<float> (inner.getX() + n * bw + 1.0f, inner.getBottom() - v * inner.getHeight(),
                                         jmax (1.0f, bw - 2.0f), v * inner.getHeight());
            g.setColour (accent.withAlpha (0.25f + 0.75f * v));
            g.fillRect (bar);
            if (n % 8 == 0)
            {
                g.setColour (Colours::dim.withAlpha (0.7f));
                g.setFont (font (9.0f));
                g.drawText (String (n + 1), Rectangle<float> (inner.getX() + n * bw, inner.getY(), 20.0f, 10.0f), Justification::topLeft);
            }
        }

        // Vista previa del ciclo de onda resultante
        constexpr int N = 160;
        std::array<float, N> wave {};
        float peak = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            float acc = 0.0f;
            const float ph = MathConstants<float>::twoPi * (float) i / (float) N;
            for (int n = 0; n < kNumHarmonics; ++n)
                if (values[(size_t) n] > 0.0f)
                    acc += values[(size_t) n] * std::sin (ph * (float) (n + 1));
            wave[(size_t) i] = acc;
            peak = jmax (peak, std::abs (acc));
        }
        if (peak > 1.0e-5f)
        {
            auto pv = Rectangle<float> (inner.getRight() - 120.0f, inner.getY() + 14.0f, 116.0f, 46.0f);
            g.setColour (Colours::bg.withAlpha (0.75f));
            g.fillRoundedRectangle (pv, 4.0f);
            Path p;
            for (int i = 0; i < N; ++i)
            {
                const float x = pv.getX() + 4.0f + (pv.getWidth() - 8.0f) * (float) i / (float) (N - 1);
                const float y = pv.getCentreY() - wave[(size_t) i] / peak * (pv.getHeight() * 0.4f);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (Colours::text.withAlpha (0.9f));
            g.strokePath (p, PathStrokeType (1.3f));
        }

        // Titulo
        g.setColour (Colours::dim);
        g.setFont (font (11.0f));
        g.drawText (U ("Dibuja con el ratón (clic derecho borra). Se usa con la forma \"Dibujada\"."),
                    Rectangle<int> (0, 0, getWidth() - 4 * 54 - 8, 24), Justification::centredLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! barsArea.contains (e.getPosition())) return;
        dragging = true;
        erasing = e.mods.isRightButtonDown();
        lastIndex = -1;
        editAt (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) editAt (e.position); }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        pushIfDirty();
    }

private:
    void editAt (juce::Point<float> pos)
    {
        auto inner = barsArea.toFloat().reduced (6.0f, 6.0f);
        const float bw = inner.getWidth() / (float) kNumHarmonics;
        const int idx = juce::jlimit (0, kNumHarmonics - 1, (int) ((pos.x - inner.getX()) / bw));
        const float val = erasing ? 0.0f : juce::jlimit (0.0f, 1.0f, (inner.getBottom() - pos.y) / inner.getHeight());

        if (lastIndex >= 0 && lastIndex != idx)
        {
            // Interpola para no dejar huecos al arrastrar rapido
            const int step = idx > lastIndex ? 1 : -1;
            for (int n = lastIndex + step; n != idx; n += step)
            {
                const float t = (float) (n - lastIndex) / (float) (idx - lastIndex);
                values[(size_t) n] = lastValue + t * (val - lastValue);
            }
        }
        values[(size_t) idx] = val;
        lastIndex = idx;
        lastValue = val;
        markEdited();
    }

    void applyShape (int shape)
    {
        juce::Random rnd;
        for (int n = 0; n < kNumHarmonics; ++n)
        {
            const int h = n + 1;
            float v = 0.0f;
            switch (shape)
            {
                case 0: v = h == 1 ? 1.0f : 0.0f; break;
                case 1: v = 1.0f / (float) h; break;
                case 2: v = (h % 2 == 1) ? 1.0f / (float) h : 0.0f; break;
                case 3: { static const int bars[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                          static const float amps[] = { 1.0f, 0.85f, 0.6f, 0.55f, 0.4f, 0.35f, 0.15f, 0.15f, 0.1f };
                          for (int b = 0; b < 9; ++b) if (bars[b] == h) v = amps[b];
                          break; }
                case 4: v = rnd.nextFloat() * std::pow ((float) h, -0.6f) * (rnd.nextFloat() < 0.65f ? 1.0f : 0.0f); break;
                default: v = 0.0f; break;
            }
            values[(size_t) n] = juce::jlimit (0.0f, 1.0f, v);
        }
        if (shape == 4) values[0] = juce::jmax (values[0], 0.6f);
        markEdited();
        pushIfDirty();
    }

    void markEdited()
    {
        dirty = true;
        if (onUserEdit) onUserEdit (selected);
        repaint();
    }

    CuarzoProcessor& proc;
    std::array<juce::TextButton, kNumOscs> tabs;
    std::array<juce::TextButton, 6> shapes;
    juce::Rectangle<int> barsArea;
    Harmonics values {};
    int selected = 0, lastIndex = -1;
    float lastValue = 0.0f;
    bool dirty = false, dragging = false, erasing = false;
};
} // namespace cuarzo::ui
