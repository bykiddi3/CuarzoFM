#include "Presets.h"
#include "PluginProcessor.h"

namespace cuarzo::presets
{
namespace
{
    struct Setter
    {
        CuarzoProcessor& proc;

        void set (const juce::String& id, float value)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        }

        void resetAll()
        {
            for (auto* p : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    rp->setValueNotifyingHost (rp->getDefaultValue());
        }

        void osc (int i, int wave, float ratio, float level, float fine = 0.0f, float fb = 0.0f)
        {
            int ratioIndex = 3;
            for (size_t r = 0; r < kRatios.size(); ++r)
                if (std::abs (kRatios[r] - ratio) < 1.0e-4f) ratioIndex = (int) r;

            set (ID::osc (i, "wave"),  (float) wave);
            set (ID::osc (i, "ratio"), (float) ratioIndex);
            set (ID::osc (i, "level"), level);
            set (ID::osc (i, "fine"),  fine);
            set (ID::osc (i, "fb"),    fb);
        }

        void env (const juce::String& prefix, float a, float d, float s, float r)
        {
            set (ID::env (prefix, "att"), a);
            set (ID::env (prefix, "dec"), d);
            set (ID::env (prefix, "sus"), s);
            set (ID::env (prefix, "rel"), r);
        }

        void harmonics (int i, std::initializer_list<std::pair<int, float>> partials)
        {
            CuarzoProcessor::Harmonics h {};
            for (auto [n, a] : partials)
                if (n >= 1 && n <= kNumHarmonics) h[(size_t) n - 1] = a;
            proc.setHarmonics (i, h);
        }

        void sawHarmonics (int i)
        {
            CuarzoProcessor::Harmonics h {};
            for (int n = 0; n < kNumHarmonics; ++n) h[(size_t) n] = 1.0f / (float) (n + 1);
            proc.setHarmonics (i, h);
        }
    };

    const juce::StringArray names {
        "Init",
        "Piano electrico FM",
        "Bajo FM saturado",
        "Campanas de cristal",
        "Pad sustractivo",
        "Metales FM",
        "Organo aditivo",
        "LFO a velocidad de audio",
    };
}

int count() { return names.size(); }
juce::String name (int index) { return juce::isPositiveAndBelow (index, names.size()) ? names[index] : juce::String(); }

void apply (int index, CuarzoProcessor& proc)
{
    Setter s { proc };
    s.resetAll();
    for (int i = 0; i < kNumOscs; ++i) s.sawHarmonics (i);

    switch (index)
    {
        case 1: // Piano electrico (algoritmo 6: B>A | D>C)
            s.set (ID::algorithm, 6);
            s.osc (0, waveSine, 1.0f, 0.80f);
            s.osc (1, waveSine, 14.0f, 0.30f);
            s.osc (2, waveSine, 1.0f, 0.55f, 3.0f);
            s.osc (3, waveSine, 1.0f, 0.50f, -3.0f, 0.15f);
            s.env (ID::envOsc (0), 0.002f, 2.5f, 0.0f, 0.6f);
            s.env (ID::envOsc (1), 0.001f, 0.35f, 0.0f, 0.3f);
            s.env (ID::envOsc (2), 0.002f, 3.5f, 0.0f, 0.7f);
            s.env (ID::envOsc (3), 0.002f, 1.2f, 0.12f, 0.5f);
            s.set (ID::velSens, 0.8f);
            s.set (ID::fltCutoff, 14000.0f);
            s.set (ID::lfoWave, LFO::sine); s.set (ID::lfoRate, 4.5f); s.set (ID::lfoAmp, 0.12f);
            break;

        case 2: // Bajo FM (algoritmo 1) con filtro saturado y overdrive
            s.set (ID::algorithm, 1);
            s.osc (0, waveSine, 1.0f, 0.90f);
            s.osc (1, waveSine, 1.0f, 0.55f, 0.0f, 0.45f);
            s.osc (2, waveSine, 2.0f, 0.25f);
            s.osc (3, waveSine, 1.0f, 0.0f);
            s.env (ID::envOsc (0), 0.001f, 0.8f, 0.65f, 0.08f);
            s.env (ID::envOsc (1), 0.001f, 0.25f, 0.25f, 0.08f);
            s.env (ID::envOsc (2), 0.001f, 0.12f, 0.0f, 0.08f);
            s.set (ID::fltMode, LadderFilter::lp24);
            s.set (ID::fltCutoff, 900.0f); s.set (ID::fltReso, 0.35f); s.set (ID::fltDrive, 0.45f);
            s.set (ID::fltEnv, 0.45f); s.set (ID::fltKey, 0.5f);
            s.env (ID::envFilter, 0.001f, 0.35f, 0.15f, 0.1f);
            s.set (ID::dstType, Distortion::overdrive); s.set (ID::dstDrive, 0.3f); s.set (ID::dstMix, 0.6f);
            s.set (ID::master, -9.0f);
            break;

        case 3: // Campanas (algoritmo 6, relaciones inarmonicas)
            s.set (ID::algorithm, 6);
            s.osc (0, waveSine, 1.0f, 0.75f);
            s.osc (1, waveSine, 3.5f, 0.45f);
            s.osc (2, waveSine, 2.0f, 0.45f, 7.0f);
            s.osc (3, waveSine, 7.0f, 0.35f);
            s.env (ID::envOsc (0), 0.001f, 6.0f, 0.0f, 4.0f);
            s.env (ID::envOsc (1), 0.001f, 3.0f, 0.0f, 3.0f);
            s.env (ID::envOsc (2), 0.001f, 4.5f, 0.0f, 3.5f);
            s.env (ID::envOsc (3), 0.001f, 1.8f, 0.0f, 2.0f);
            s.set (ID::velSens, 0.7f);
            break;

        case 4: // Pad sustractivo (algoritmo 11, osciladores en paralelo)
            s.set (ID::algorithm, 11);
            s.osc (0, waveSaw, 1.0f, 0.60f, -9.0f);
            s.osc (1, waveSaw, 1.0f, 0.60f, 9.0f);
            s.osc (2, waveSquare, 0.5f, 0.40f);
            s.osc (3, waveNoise, 1.0f, 0.04f);
            for (int i = 0; i < kNumOscs; ++i) s.env (ID::envOsc (i), 0.9f, 1.0f, 0.85f, 2.2f);
            s.set (ID::fltMode, LadderFilter::lp24);
            s.set (ID::fltCutoff, 700.0f); s.set (ID::fltReso, 0.4f); s.set (ID::fltDrive, 0.15f); s.set (ID::fltEnv, 0.35f);
            s.env (ID::envFilter, 1.5f, 2.5f, 0.35f, 2.0f);
            s.set (ID::lfoWave, LFO::triangle); s.set (ID::lfoRate, 0.25f); s.set (ID::lfoCutoff, 0.2f); s.set (ID::lfoRetrig, 0.0f);
            s.set (ID::velSens, 0.3f);
            break;

        case 5: // Metales (algoritmo 2) con caida de tono
            s.set (ID::algorithm, 2);
            s.osc (0, waveSine, 1.0f, 0.80f);
            s.osc (1, waveSine, 1.0f, 0.50f, 0.0f, 0.3f);
            s.osc (2, waveSine, 2.0f, 0.20f);
            s.osc (3, waveSine, 1.0f, 0.18f, 5.0f);
            s.env (ID::envOsc (0), 0.04f, 0.3f, 0.8f, 0.2f);
            s.env (ID::envOsc (1), 0.07f, 0.4f, 0.65f, 0.25f);
            s.env (ID::envOsc (2), 0.05f, 0.3f, 0.4f, 0.2f);
            s.env (ID::envOsc (3), 0.05f, 0.3f, 0.5f, 0.2f);
            s.set (ID::pitAmount, -1.2f);
            s.env (ID::envPitch, 0.001f, 0.09f, 0.0f, 0.1f);
            s.set (ID::lfoWave, LFO::sine); s.set (ID::lfoRate, 5.5f); s.set (ID::lfoPitch, 0.12f);
            s.env (ID::envLfo, 0.6f, 0.1f, 1.0f, 0.3f);
            s.set (ID::fltCutoff, 9000.0f);
            break;

        case 6: // Organo aditivo (forma dibujada tipo registros/drawbars)
            s.set (ID::algorithm, 11);
            s.harmonics (0, { { 1, 1.0f }, { 2, 0.8f }, { 3, 0.6f }, { 4, 0.5f }, { 6, 0.35f }, { 8, 0.3f }, { 10, 0.12f }, { 12, 0.1f }, { 16, 0.08f } });
            s.osc (0, waveDrawn, 1.0f, 0.75f);
            s.osc (1, waveSine, 0.5f, 0.35f);
            s.osc (2, waveSine, 1.0f, 0.0f);
            s.osc (3, waveSine, 1.0f, 0.0f);
            s.env (ID::envOsc (0), 0.004f, 0.1f, 1.0f, 0.08f);
            s.env (ID::envOsc (1), 0.004f, 0.1f, 1.0f, 0.08f);
            s.set (ID::lfoWave, LFO::sine); s.set (ID::lfoRate, 6.2f); s.set (ID::lfoPitch, 0.08f); s.set (ID::lfoAmp, 0.15f);
            s.set (ID::dstType, Distortion::saturation); s.set (ID::dstDrive, 0.35f);
            break;

        case 7: // LFO a velocidad de audio siguiendo la nota (modulacion en anillo + FM)
            s.set (ID::algorithm, 11);
            s.osc (0, waveSaw, 1.0f, 0.70f);
            s.osc (1, waveSine, 1.0f, 0.0f);
            s.osc (2, waveSine, 1.0f, 0.0f);
            s.osc (3, waveSine, 1.0f, 0.0f);
            s.env (ID::envOsc (0), 0.002f, 0.6f, 0.6f, 0.4f);
            s.set (ID::lfoWave, LFO::sine); s.set (ID::lfoKey, 1.0f); s.set (ID::lfoRate, 392.4f);
            s.set (ID::lfoAmp, 0.85f); s.set (ID::lfoPitch, 1.5f);
            s.env (ID::envLfo, 0.001f, 0.8f, 0.3f, 0.4f);
            s.set (ID::fltMode, LadderFilter::lp12); s.set (ID::fltCutoff, 5000.0f); s.set (ID::fltReso, 0.5f);
            s.set (ID::dstType, Distortion::wavefolder); s.set (ID::dstDrive, 0.35f); s.set (ID::dstMix, 0.7f);
            break;

        case 0:
        default:
            break;
    }
}
} // namespace cuarzo::presets
