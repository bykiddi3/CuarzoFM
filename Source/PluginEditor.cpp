#include "PluginEditor.h"
#include "Presets.h"

using namespace cuarzo;
using namespace cuarzo::ui;

namespace
{
    constexpr int kW = 1180, kH = 772;
    constexpr int kTitleH = 28;
    constexpr int kKnobH = 84;
}

CuarzoEditor::CuarzoEditor (CuarzoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), algoDisplay (p.apvts), harmEditor (p)
{
    setLookAndFeel (&look);

    // ---- Osciladores ----
    for (int i = 0; i < kNumOscs; ++i)
    {
        const auto c = ui::Colours::osc (i);
        auto& o = oscUI[(size_t) i];
        o.wave  = &combo (ID::osc (i, "wave"), waveNames());
        o.ratio = &knob (ID::osc (i, "ratio"), U ("RELACIÓN"), c);
        o.fine  = &knob (ID::osc (i, "fine"),  U ("FINO"), c);
        o.level = &knob (ID::osc (i, "level"), U ("NIVEL"), c);
        o.fb    = &knob (ID::osc (i, "fb"),    U ("FEEDBACK"), c);
        o.a = &knob (ID::env (ID::envOsc (i), "att"), "A", c);
        o.d = &knob (ID::env (ID::envOsc (i), "dec"), "D", c);
        o.s = &knob (ID::env (ID::envOsc (i), "sus"), "S", c);
        o.r = &knob (ID::env (ID::envOsc (i), "rel"), "R", c);
    }

    // ---- Filtro ----
    {
        const auto c = ui::Colours::filter;
        fMode  = &combo (ID::fltMode, filterModeNames());
        fCut   = &knob (ID::fltCutoff, "CORTE", c);
        fRes   = &knob (ID::fltReso,   "RESONANCIA", c);
        fDrive = &knob (ID::fltDrive,  U ("SATURACIÓN"), c);
        fEnv   = &knob (ID::fltEnv,    "ENV", c);
        fKey   = &knob (ID::fltKey,    "TECLADO", c);
        fA = &knob (ID::env (ID::envFilter, "att"), "A", c);
        fD = &knob (ID::env (ID::envFilter, "dec"), "D", c);
        fS = &knob (ID::env (ID::envFilter, "sus"), "S", c);
        fR = &knob (ID::env (ID::envFilter, "rel"), "R", c);
    }

    // ---- Envolvente de tono ----
    {
        const auto c = ui::Colours::pitch;
        pAmt = &knob (ID::pitAmount, "CANTIDAD", c);
        pA = &knob (ID::env (ID::envPitch, "att"), "A", c);
        pD = &knob (ID::env (ID::envPitch, "dec"), "D", c);
        pS = &knob (ID::env (ID::envPitch, "sus"), "S", c);
        pR = &knob (ID::env (ID::envPitch, "rel"), "R", c);
    }

    // ---- LFO ----
    {
        const auto c = ui::Colours::lfo;
        lWave   = &combo (ID::lfoWave, lfoWaveNames());
        lKey    = &toggle (ID::lfoKey, "Seguir nota", c);
        lRetrig = &toggle (ID::lfoRetrig, "Retrig", c);
        lRate   = &knob (ID::lfoRate,   "VELOCIDAD", c);
        lPitch  = &knob (ID::lfoPitch,  U ("→ TONO"), c);
        lCut    = &knob (ID::lfoCutoff, U ("→ FILTRO"), c);
        lAmp    = &knob (ID::lfoAmp,    U ("→ AMP"), c);
        lFm     = &knob (ID::lfoFm,     U ("→ ÍNDICE FM"), c);
        lA = &knob (ID::env (ID::envLfo, "att"), "A", c);
        lD = &knob (ID::env (ID::envLfo, "dec"), "D", c);
        lS = &knob (ID::env (ID::envLfo, "sus"), "S", c);
        lR = &knob (ID::env (ID::envLfo, "rel"), "R", c);
        lKey->setTooltip (U ("La velocidad del LFO sigue la nota tocada (Do4 = valor de VELOCIDAD). Útil para FM / AM a velocidad de audio."));
    }

    // ---- Distorsion / salida ----
    {
        const auto c = ui::Colours::dist;
        dType  = &combo (ID::dstType, distTypeNames());
        dDrive = &knob (ID::dstDrive, "CANTIDAD", c);
        dMix   = &knob (ID::dstMix,   "MEZCLA", c);
        master = &knob (ID::master,   "VOLUMEN", ui::Colours::text);
        vel    = &knob (ID::velSens,  "SENS. VEL", ui::Colours::text);
        bend   = &knob (ID::bendRange, "PITCH BEND", ui::Colours::text);
    }

    // ---- Presets ----
    for (int i = 0; i < presets::count(); ++i)
        presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + presets::name (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
        {
            proc.setCurrentProgram (idx);
            harmEditor.refreshFromProcessor();
        }
    };
    addAndMakeVisible (presetBox);

    addAndMakeVisible (algoDisplay);

    harmEditor.onUserEdit = [this] (int osc)
    {
        if (auto* param = proc.apvts.getParameter (ID::osc (osc, "wave")))
            if ((int) std::round (param->convertFrom0to1 (param->getValue())) != waveDrawn)
                param->setValueNotifyingHost (param->convertTo0to1 ((float) waveDrawn));
    };
    addAndMakeVisible (harmEditor);

    setSize (kW, kH);
    startTimerHz (30);
}

CuarzoEditor::~CuarzoEditor()
{
    stopTimer();
    harmEditor.pushIfDirty();
    setLookAndFeel (nullptr);
}

CuarzoEditor::Knob& CuarzoEditor::knob (const juce::String& id, const juce::String& label, juce::Colour accent)
{
    knobs.push_back (std::make_unique<Knob> (proc.apvts, id, label, accent));
    addAndMakeVisible (*knobs.back());
    return *knobs.back();
}

juce::ComboBox& CuarzoEditor::combo (const juce::String& id, const juce::StringArray& items)
{
    auto cb = std::make_unique<juce::ComboBox>();
    juce::StringArray display;
    for (auto& s : items) // nombres visibles con acentos
        display.add (s == "Triangulo" ? U ("Triángulo") : s == "Saturacion" ? U ("Saturación") : s);
    cb->addItemList (display, 1);
    addAndMakeVisible (*cb);
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, *cb));
    combos.push_back (std::move (cb));
    return *combos.back();
}

juce::ToggleButton& CuarzoEditor::toggle (const juce::String& id, const juce::String& text, juce::Colour accent)
{
    auto tb = std::make_unique<juce::ToggleButton> (text);
    tb->setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (*tb);
    toggleAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, *tb));
    toggles.push_back (std::move (tb));
    return *toggles.back();
}

void CuarzoEditor::timerCallback()
{
    harmEditor.pushIfDirty();

    if (const int v = proc.getHarmonicsVersion(); v != lastHarmVersion)
    {
        lastHarmVersion = v;
        harmEditor.refreshFromProcessor();
    }

    if (const int a = algoDisplay.getAlgorithm(); a != lastAlgorithm)
    {
        lastAlgorithm = a;
        repaint();
    }

    static int tick = 0;
    if (++tick % 6 == 0)
        algoDisplay.repaint();

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

// ------------------------------------------------------------------------------------
void CuarzoEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title,
                              juce::Colour accent, const juce::String& badge)
{
    auto rf = r.toFloat();
    g.setColour (ui::Colours::panel);
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (ui::Colours::panelEdge);
    g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);

    // Acento lateral
    g.setColour (accent);
    g.fillRoundedRectangle (rf.getX() + 10.0f, rf.getY() + 9.0f, 3.0f, 12.0f, 1.5f);

    g.setColour (ui::Colours::text);
    g.setFont (font (13.0f, true));
    g.drawText (title, r.getX() + 19, r.getY() + 4, 220, 22, juce::Justification::centredLeft);

    if (badge.isNotEmpty())
    {
        const int tw = (int) juce::GlyphArrangement::getStringWidth (font (13.0f, true), title) + 28;
        auto b = juce::Rectangle<float> ((float) (r.getX() + tw), (float) r.getY() + 8.0f, 74.0f, 15.0f);
        g.setColour (accent.withAlpha (0.18f));
        g.fillRoundedRectangle (b, 7.0f);
        g.setColour (accent);
        g.setFont (font (9.5f, true));
        g.drawText (badge, b, juce::Justification::centred);
    }
}

void CuarzoEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff181b22), 0, 0, ui::Colours::bg, 0, (float) kH, false));
    g.fillAll();

    // Cabecera
    g.setColour (ui::Colours::text);
    g.setFont (font (24.0f, true));
    g.drawText ("CUARZO", 18, 10, 140, 32, juce::Justification::centredLeft);
    g.setColour (ui::Colours::oscA);
    g.setFont (font (24.0f, false));
    g.drawText ("FM", 128, 10, 60, 32, juce::Justification::centredLeft);
    g.setColour (ui::Colours::dim);
    g.setFont (font (11.5f));
    g.drawText (U ("4 OSCILADORES · 11 ALGORITMOS · FM / SUSTRACTIVA / ADITIVA"), 180, 10, 460, 32, juce::Justification::centredLeft);
    g.drawText ("PRESET", presetBox.getX() - 60, presetBox.getY(), 54, presetBox.getHeight(), juce::Justification::centredRight);

    // Paneles
    drawPanel (g, rAlgo, "ALGORITMO", ui::Colours::text);

    const auto& alg = algorithms()[(size_t) juce::jlimit (0, kNumAlgorithms - 1, algoDisplay.getAlgorithm() - 1)];
    for (int i = 0; i < kNumOscs; ++i)
        drawPanel (g, rOsc[i], "OSC " + oscLetter (i), ui::Colours::osc (i), alg.isCarrier (i) ? "PORTADOR" : "MODULADOR");

    drawPanel (g, rHarm,   U ("ARMÓNICOS"), ui::Colours::osc (0));
    drawPanel (g, rFilter, "FILTRO ANALOG", ui::Colours::filter);
    drawPanel (g, rPitch,  "ENV. DE TONO", ui::Colours::pitch);
    drawPanel (g, rLfo,    "LFO", ui::Colours::lfo);
    drawPanel (g, rOut,    U ("DISTORSIÓN · SALIDA"), ui::Colours::dist);

    // Etiquetas de filas de envolvente
    g.setColour (ui::Colours::dim.withAlpha (0.6f));
    g.setFont (font (9.5f, true));
    auto rowLabel = [&g] (juce::Rectangle<int> panel, const juce::String& txt)
    {
        g.drawText (txt, panel.getRight() - 110, panel.getBottom() - 16, 100, 14, juce::Justification::centredRight);
    };
    rowLabel (rFilter, "ENV FILTRO");
    rowLabel (rLfo, "ENV LFO");
}

void CuarzoEditor::layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps)
{
    const int n = (int) comps.size();
    if (n == 0) return;
    const int w = area.getWidth() / n;
    for (auto* c : comps)
    {
        auto cell = area.removeFromLeft (w);
        if (c != nullptr) c->setBounds (cell.reduced (2, 0));
    }
}

void CuarzoEditor::resized()
{
    presetBox.setBounds (kW - 290, 14, 272, 24);

    rAlgo = { 10, 58, 200, 244 };
    const int oscW = (kW - 10 - 218 - 3 * 8) / 4;
    for (int i = 0; i < kNumOscs; ++i)
        rOsc[i] = { 218 + i * (oscW + 8), 58, oscW, 244 };

    rHarm   = { 10, 310, 572, 222 };
    rFilter = { 590, 310, kW - 10 - 590, 222 };
    rPitch  = { 10, 540, 290, 222 };
    rLfo    = { 308, 540, 574, 222 };
    rOut    = { 890, 540, kW - 10 - 890, 222 };

    auto content = [] (juce::Rectangle<int> r) { return r.reduced (8, 8).withTrimmedTop (kTitleH - 4); };
    auto titleRight = [] (juce::Rectangle<int> r, int w) { return juce::Rectangle<int> (r.getRight() - w - 10, r.getY() + 6, w, 20); };

    algoDisplay.setBounds (content (rAlgo).withTrimmedTop (-2));

    for (int i = 0; i < kNumOscs; ++i)
    {
        auto& o = oscUI[(size_t) i];
        auto c = content (rOsc[i]);
        o.wave->setBounds (c.removeFromTop (22).reduced (2, 0));
        c.removeFromTop (8);
        layoutRow (c.removeFromTop (kKnobH), { o.ratio, o.fine, o.level, o.fb });
        c.removeFromTop (8);
        layoutRow (c.removeFromTop (kKnobH), { o.a, o.d, o.s, o.r });
    }

    harmEditor.setBounds (content (rHarm).withTrimmedTop (-2));

    {
        fMode->setBounds (titleRight (rFilter, 130));
        auto c = content (rFilter);
        c.removeFromTop (6);
        layoutRow (c.removeFromTop (kKnobH), { fCut, fRes, fDrive, fEnv, fKey });
        c.removeFromTop (8);
        layoutRow (c.removeFromTop (kKnobH), { fA, fD, fS, fR, nullptr });
    }
    {
        auto c = content (rPitch);
        c.removeFromTop (6);
        layoutRow (c.removeFromTop (kKnobH), { pAmt, nullptr, nullptr, nullptr });
        c.removeFromTop (8);
        layoutRow (c.removeFromTop (kKnobH), { pA, pD, pS, pR });
    }
    {
        auto t = titleRight (rLfo, 360);
        lRetrig->setBounds (t.removeFromRight (80));
        lKey->setBounds (t.removeFromRight (115));
        t.removeFromRight (8);
        lWave->setBounds (t.removeFromRight (150));
        auto c = content (rLfo);
        c.removeFromTop (6);
        layoutRow (c.removeFromTop (kKnobH), { lRate, lPitch, lCut, lAmp, lFm });
        c.removeFromTop (8);
        layoutRow (c.removeFromTop (kKnobH), { lA, lD, lS, lR, nullptr });
    }
    {
        dType->setBounds (titleRight (rOut, 112));
        auto c = content (rOut);
        c.removeFromTop (6);
        layoutRow (c.removeFromTop (kKnobH), { dDrive, dMix, master });
        c.removeFromTop (8);
        layoutRow (c.removeFromTop (kKnobH), { vel, bend, nullptr });
    }
}
