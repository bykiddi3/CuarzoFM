#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace cuarzo
{
constexpr int kNumOscs      = 4;
constexpr int kNumHarmonics = 64;
constexpr int kNumAlgorithms = 11;

// Relaciones de frecuencia disponibles para cada oscilador (estilo FM clasico)
inline const std::array<float, 22> kRatios { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f,
                                             6.0f,  7.0f, 8.0f,  9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f };

enum Wave { waveSine = 0, waveTriangle, waveSquare, waveSaw, waveNoise, waveDrawn, numWaves };

inline juce::StringArray waveNames()      { return { "Seno", "Triangulo", "Cuadrada", "Diente de sierra", "Ruido blanco", "Dibujada" }; }
inline juce::StringArray filterModeNames(){ return { "LP 24 dB", "LP 12 dB", "BP 12 dB", "HP 12 dB", "HP 24 dB", "Notch" }; }
inline juce::StringArray lfoWaveNames()   { return { "Seno", "Triangulo", "Diente de sierra", "Cuadrada", "S&H aleatorio" }; }
inline juce::StringArray distTypeNames()  { return { "Apagado", "Overdrive", "Saturacion", "Fuzz", "Wavefolder", "Bitcrusher" }; }

inline juce::String oscLetter (int i)     { return juce::String::charToString ((juce::juce_wchar) ('A' + i)); }

// ---------------- Identificadores de parametros ----------------
namespace ID
{
    inline juce::String osc (int i, const char* name) { return "osc" + oscLetter (i) + "_" + name; }

    inline const juce::String algorithm   { "algorithm" };
    inline const juce::String master      { "master" };
    inline const juce::String velSens     { "vel_sens" };
    inline const juce::String bendRange   { "bend_range" };

    inline const juce::String fltMode     { "flt_mode" };
    inline const juce::String fltCutoff   { "flt_cutoff" };
    inline const juce::String fltReso     { "flt_reso" };
    inline const juce::String fltDrive    { "flt_drive" };
    inline const juce::String fltEnv      { "flt_env" };
    inline const juce::String fltKey      { "flt_key" };

    inline const juce::String pitAmount   { "pit_amount" };

    inline const juce::String lfoWave     { "lfo_wave" };
    inline const juce::String lfoRate     { "lfo_rate" };
    inline const juce::String lfoKey      { "lfo_key" };
    inline const juce::String lfoRetrig   { "lfo_retrig" };
    inline const juce::String lfoPitch    { "lfo_pitch" };
    inline const juce::String lfoCutoff   { "lfo_cutoff" };
    inline const juce::String lfoAmp      { "lfo_amp" };
    inline const juce::String lfoFm       { "lfo_fm" };

    inline const juce::String dstType     { "dst_type" };
    inline const juce::String dstDrive    { "dst_drive" };
    inline const juce::String dstMix      { "dst_mix" };

    // Envolventes: prefijo + "_att/_dec/_sus/_rel"
    inline juce::String env (const juce::String& prefix, const char* stage) { return prefix + "_" + stage; }
    inline const juce::String envFilter { "fenv" };
    inline const juce::String envPitch  { "penv" };
    inline const juce::String envLfo    { "lenv" };
    inline juce::String envOsc (int i)  { return "osc" + oscLetter (i) + "_env"; }
}

// ---------------- Creacion del layout ----------------
inline juce::NormalisableRange<float> timeRange()
{
    juce::NormalisableRange<float> r (0.001f, 20.0f);
    r.setSkewForCentre (0.6f);
    return r;
}

inline void addEnvelope (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                         const juce::String& prefix, const juce::String& name,
                         float a, float d, float s, float r)
{
    using namespace juce;
    auto secs = AudioParameterFloatAttributes().withLabel ("s")
                    .withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? String (v * 1000.0f, 0) + " ms" : String (v, 2) + " s"; });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::env (prefix, "att"), 1 }, name + " Ataque",     timeRange(), a, secs));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::env (prefix, "dec"), 1 }, name + " Decaimiento", timeRange(), d, secs));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::env (prefix, "sus"), 1 }, name + " Sostenido",  NormalisableRange<float> (0.0f, 1.0f), s,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v * 100.0f, 0) + " %"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::env (prefix, "rel"), 1 }, name + " Liberacion", timeRange(), r, secs));
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v * 100.0f, 0) + " %"; });
    auto bipct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v * 100.0f, 0) + " %"; });
    auto semis = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 2) + " st"; });

    // ---- Global ----
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ID::algorithm, 1 }, "Algoritmo", 1, kNumAlgorithms, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::master, 1 }, "Volumen", NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -6.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::velSens, 1 }, "Sensibilidad velocidad", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ID::bendRange, 1 }, "Rango pitch bend", 0, 24, 2));

    // ---- Osciladores ----
    StringArray ratioNames;
    for (auto r : kRatios) ratioNames.add ("x" + String (r, r < 1.0f ? 2 : (std::abs (r - std::floor (r)) < 1.0e-4f ? 0 : 1)));

    for (int i = 0; i < kNumOscs; ++i)
    {
        const auto L = "Osc " + oscLetter (i);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::osc (i, "wave"), 1 }, L + " Forma de onda", waveNames(), 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::osc (i, "ratio"), 1 }, L + " Relacion", ratioNames, 3));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::osc (i, "fine"), 1 }, L + " Afinacion fina",
                                                           NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " ct"; })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::osc (i, "level"), 1 }, L + " Nivel / Indice",
                                                           NormalisableRange<float> (0.0f, 1.0f), i == 0 ? 0.8f : 0.0f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::osc (i, "fb"), 1 }, L + " Feedback",
                                                           NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        addEnvelope (layout, ID::envOsc (i), L, 0.005f, 0.4f, 0.8f, 0.3f);
    }

    // ---- Filtro ----
    NormalisableRange<float> cutRange (20.0f, 20000.0f);
    cutRange.setSkewForCentre (1000.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::fltMode, 1 }, "Filtro Modo", filterModeNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::fltCutoff, 1 }, "Filtro Corte", cutRange, 16000.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1000.0f ? String (v, 0) + " Hz" : String (v / 1000.0f, 2) + " kHz"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::fltReso, 1 }, "Filtro Resonancia", NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::fltDrive, 1 }, "Filtro Saturacion", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::fltEnv, 1 }, "Filtro Cantidad env", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::fltKey, 1 }, "Filtro Seguimiento teclado", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    addEnvelope (layout, ID::envFilter, "Filtro", 0.005f, 0.5f, 0.5f, 0.4f);

    // ---- Envolvente de tono ----
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::pitAmount, 1 }, "Tono Cantidad env",
                                                       NormalisableRange<float> (-48.0f, 48.0f, 0.01f, 0.4f, true), 0.0f, semis));
    addEnvelope (layout, ID::envPitch, "Tono", 0.001f, 0.15f, 0.0f, 0.2f);

    // ---- LFO ----
    NormalisableRange<float> rateRange (0.01f, 2000.0f);
    rateRange.setSkewForCentre (8.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::lfoWave, 1 }, "LFO Forma", lfoWaveNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::lfoRate, 1 }, "LFO Velocidad", rateRange, 5.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 10.0f ? String (v, 2) + " Hz" : String (v, 1) + " Hz"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ID::lfoKey, 1 }, "LFO Seguir nota (audio)", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ID::lfoRetrig, 1 }, "LFO Reiniciar con nota", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::lfoPitch, 1 }, "LFO a Tono",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.01f, 0.4f, true), 0.0f, semis));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::lfoCutoff, 1 }, "LFO a Filtro", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::lfoAmp, 1 }, "LFO a Amplitud", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::lfoFm, 1 }, "LFO a Indice FM", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipct));
    addEnvelope (layout, ID::envLfo, "LFO", 0.001f, 0.1f, 1.0f, 0.5f);

    // ---- Distorsion ----
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::dstType, 1 }, "Distorsion Tipo", distTypeNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::dstDrive, 1 }, "Distorsion Cantidad", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::dstMix, 1 }, "Distorsion Mezcla", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    return layout;
}
} // namespace cuarzo
