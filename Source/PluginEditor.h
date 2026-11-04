#pragma once

#include "PluginProcessor.h"
#include "UI/CuarzoLook.h"
#include "UI/Knob.h"
#include "UI/AlgorithmDisplay.h"
#include "UI/HarmonicEditor.h"

class CuarzoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CuarzoEditor (CuarzoProcessor&);
    ~CuarzoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Knob = cuarzo::ui::Knob;
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;
    Knob& knob (const juce::String& id, const juce::String& label, juce::Colour accent);
    juce::ComboBox& combo (const juce::String& id, const juce::StringArray& items);
    juce::ToggleButton& toggle (const juce::String& id, const juce::String& text, juce::Colour accent);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, juce::Colour accent, const juce::String& badge = {});
    static void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps);

    CuarzoProcessor& proc;
    cuarzo::ui::CuarzoLook look;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> toggleAtts;

    juce::ComboBox presetBox;
    cuarzo::ui::AlgorithmDisplay algoDisplay;
    cuarzo::ui::HarmonicEditor harmEditor;

    struct OscUI { juce::ComboBox* wave; Knob *ratio, *fine, *level, *fb, *a, *d, *s, *r; };
    std::array<OscUI, cuarzo::kNumOscs> oscUI {};

    Knob *fCut, *fRes, *fDrive, *fEnv, *fKey, *fA, *fD, *fS, *fR;
    juce::ComboBox* fMode;
    Knob *pAmt, *pA, *pD, *pS, *pR;
    juce::ComboBox* lWave;
    juce::ToggleButton *lKey, *lRetrig;
    Knob *lRate, *lPitch, *lCut, *lAmp, *lFm, *lA, *lD, *lS, *lR;
    juce::ComboBox* dType;
    Knob *dDrive, *dMix, *master, *vel, *bend;

    juce::Rectangle<int> rAlgo, rOsc[cuarzo::kNumOscs], rHarm, rFilter, rPitch, rLfo, rOut;
    int lastAlgorithm = -1, lastHarmVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CuarzoEditor)
};
