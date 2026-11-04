#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Wavetables.h"
#include "DSP/Envelope.h"
#include "DSP/LadderFilter.h"
#include "DSP/Modulation.h"
#include "DSP/Algorithms.h"
#include "Parameters.h"

namespace cuarzo
{
struct EnvParams { float a = 0.005f, d = 0.3f, s = 0.8f, r = 0.3f; };

// Instantanea de parametros, leida una vez por bloque de audio
struct VoiceParams
{
    int algorithm = 0;

    struct Osc
    {
        int wave = waveSine;
        float ratio = 1.0f, fineCents = 0.0f, level = 0.0f, feedback = 0.0f;
        EnvParams env;
        const MipTable* drawn = nullptr;
    };
    std::array<Osc, kNumOscs> osc;

    int   fltMode = 0;
    float cutoff = 16000.0f, reso = 0.1f, drive = 0.0f, fltEnvAmt = 0.0f, fltKey = 0.0f;
    EnvParams fltEnv;

    float pitchAmt = 0.0f;
    EnvParams pitchEnv;

    int   lfoWave = 0;
    float lfoRate = 5.0f;
    bool  lfoKey = false, lfoRetrig = true;
    float lfoPitch = 0.0f, lfoCutoff = 0.0f, lfoAmp = 0.0f, lfoFm = 0.0f;
    float lfoFreePhase = 0.0f;
    EnvParams lfoEnv;

    float velSens = 0.5f;
    int   bendRange = 2;
};

struct CuarzoSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class CuarzoVoice : public juce::SynthesiserVoice
{
public:
    explicit CuarzoVoice (const VoiceParams& sharedParams, uint32_t seed);

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<CuarzoSound*> (s) != nullptr; }
    void setCurrentPlaybackSampleRate (double newRate) override;

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int controller, int newValue) override;
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;

private:
    void updateEnvelopes() noexcept;
    void resetState() noexcept;
    bool carriersFinished() const noexcept;

    const VoiceParams& params;

    double sr = 44100.0;
    int    note = 60;
    float  noteFreq = 261.63f, velocity = 1.0f, bend = 0.0f, modWheel = 0.0f;

    std::array<float, kNumOscs> phase {}, prev1 {}, prev2 {}, levelSmooth {};
    float cutoffSmooth = 16000.0f;

    std::array<Envelope, kNumOscs> oscEnv;
    Envelope fltEnv, pitchEnv, lfoEnv;
    LFO lfo;
    Noise noise;
    LadderFilter filter;
};
} // namespace cuarzo
