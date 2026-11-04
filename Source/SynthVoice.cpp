#include "SynthVoice.h"

namespace cuarzo
{
namespace
{
    constexpr float kMaxModIndex = 2.0f;   // indice FM maximo, en ciclos (= 4*pi radianes)
    constexpr float kMaxFeedback = 0.5f;   // realimentacion maxima, en ciclos
    constexpr float kLevelSmooth = 0.0025f;
    constexpr float kVoiceGain   = 0.7f;   // margen para acordes

    inline float wrap01 (float x) noexcept { return x - std::floor (x); }

    inline const MipTable* tableFor (const VoiceParams::Osc& o) noexcept
    {
        const auto& cw = ClassicWaves::get();
        switch (o.wave)
        {
            case waveTriangle: return &cw.triangle;
            case waveSquare:   return &cw.square;
            case waveSaw:      return &cw.saw;
            case waveDrawn:    return o.drawn != nullptr ? o.drawn : &cw.sine;
            case waveSine:
            default:           return &cw.sine;
        }
    }
}

CuarzoVoice::CuarzoVoice (const VoiceParams& sharedParams, uint32_t seed)
    : params (sharedParams)
{
    noise.seed (seed * 2654435761u + 1u);
    lfo.seed (seed * 40503u + 7u);
}

void CuarzoVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    if (newRate <= 0.0) return;

    sr = newRate;
    for (auto& e : oscEnv) e.setSampleRate (sr);
    fltEnv.setSampleRate (sr);
    pitchEnv.setSampleRate (sr);
    lfoEnv.setSampleRate (sr);
    filter.prepare (sr);
    resetState();
}

void CuarzoVoice::resetState() noexcept
{
    for (auto& e : oscEnv) e.reset();
    fltEnv.reset(); pitchEnv.reset(); lfoEnv.reset();
    filter.reset();
    phase.fill (0.0f); prev1.fill (0.0f); prev2.fill (0.0f);
}

void CuarzoVoice::updateEnvelopes() noexcept
{
    for (int i = 0; i < kNumOscs; ++i)
    {
        const auto& e = params.osc[(size_t) i].env;
        oscEnv[(size_t) i].setParameters (e.a, e.d, e.s, e.r);
    }
    fltEnv.setParameters   (params.fltEnv.a,   params.fltEnv.d,   params.fltEnv.s,   params.fltEnv.r);
    pitchEnv.setParameters (params.pitchEnv.a, params.pitchEnv.d, params.pitchEnv.s, params.pitchEnv.r);
    lfoEnv.setParameters   (params.lfoEnv.a,   params.lfoEnv.d,   params.lfoEnv.s,   params.lfoEnv.r);
}

void CuarzoVoice::startNote (int midiNote, float vel, juce::SynthesiserSound*, int pitchWheel)
{
    note     = midiNote;
    noteFreq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
    velocity = vel;
    pitchWheelMoved (pitchWheel);

    resetState();
    updateEnvelopes();

    for (int i = 0; i < kNumOscs; ++i)
        levelSmooth[(size_t) i] = params.osc[(size_t) i].level;
    cutoffSmooth = params.cutoff;

    lfo.reset (params.lfoRetrig ? 0.0f : params.lfoFreePhase);

    for (auto& e : oscEnv) e.noteOn();
    fltEnv.noteOn(); pitchEnv.noteOn(); lfoEnv.noteOn();
}

void CuarzoVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        for (auto& e : oscEnv) e.noteOff();
        fltEnv.noteOff(); pitchEnv.noteOff(); lfoEnv.noteOff();
    }
    else
    {
        resetState();
        clearCurrentNote();
    }
}

void CuarzoVoice::pitchWheelMoved (int newValue)
{
    bend = juce::jlimit (-1.0f, 1.0f, (float) (newValue - 8192) / 8192.0f);
}

void CuarzoVoice::controllerMoved (int controller, int newValue)
{
    if (controller == 1) // rueda de modulacion => vibrato adicional via LFO
        modWheel = (float) newValue / 127.0f;
}

bool CuarzoVoice::carriersFinished() const noexcept
{
    const auto& alg = algorithms()[(size_t) params.algorithm];
    for (int i = 0; i < kNumOscs; ++i)
    {
        if (! alg.isCarrier (i)) continue;
        const auto& e = oscEnv[(size_t) i];
        const bool silent = ! e.isActive()
                         || (e.getStage() == Envelope::Stage::sustain && e.getLevel() < 1.0e-4f);
        if (! silent) return false;
    }
    return true;
}

void CuarzoVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    updateEnvelopes();

    const auto& p   = params;
    const auto& alg = algorithms()[(size_t) p.algorithm];
    const float invSr = (float) (1.0 / sr);
    const float carrierNorm = 1.0f / std::sqrt ((float) std::max (1, alg.numCarriers()));
    const float velGain = 1.0f - p.velSens + p.velSens * velocity;

    std::array<const MipTable*, kNumOscs> tables {};
    std::array<const float*, kNumOscs> levelPtr {};
    std::array<float, kNumOscs> ratioMul {};
    for (int i = 0; i < kNumOscs; ++i)
    {
        const auto& o = p.osc[(size_t) i];
        tables[(size_t) i]   = tableFor (o);
        ratioMul[(size_t) i] = o.ratio * std::exp2 (o.fineCents / 1200.0f);
    }

    const float lfoBaseRate = p.lfoKey ? p.lfoRate * (noteFreq / 261.6256f) : p.lfoRate;
    const float lfoInc      = lfoBaseRate * invSr;
    const float lfoPitch    = p.lfoPitch + modWheel * 0.5f;
    const float keyOct      = p.fltKey * (float) (note - 60) / 12.0f;
    const float bendSemis   = bend * (float) p.bendRange;

    auto* outL = output.getWritePointer (0);
    auto* outR = output.getNumChannels() > 1 ? output.getWritePointer (1) : nullptr;

    for (int s = 0; s < numSamples; ++s)
    {
        // --- Moduladores globales de la voz ---
        const float lfoV  = lfo.next (p.lfoWave, lfoInc) * lfoEnv.next();
        const float semis = p.pitchAmt * pitchEnv.next() + lfoPitch * lfoV + bendSemis;
        const float baseInc = noteFreq * std::exp2 (semis * (1.0f / 12.0f)) * invSr;
        const float fmScale = std::max (0.0f, 1.0f + p.lfoFm * lfoV);

        // Seleccion de nivel de mipmap (cada 16 muestras es suficiente)
        if ((s & 15) == 0)
            for (int i = 0; i < kNumOscs; ++i)
                levelPtr[(size_t) i] = tables[(size_t) i]->levelFor (baseInc * ratioMul[(size_t) i]);

        // --- Osciladores: D -> C -> B -> A ---
        std::array<float, kNumOscs> opOut {};
        std::array<float, kNumOscs> depth {};
        float carrierSum = 0.0f;

        for (int i = kNumOscs - 1; i >= 0; --i)
        {
            const auto idx = (size_t) i;
            const auto& o = p.osc[idx];
            levelSmooth[idx] += (o.level - levelSmooth[idx]) * kLevelSmooth;
            const float lvl = levelSmooth[idx];
            const float env = oscEnv[idx].next();

            float mod = 0.0f;
            for (int src = i + 1; src < kNumOscs; ++src)
                if (alg.modulates (src, i))
                    mod += opOut[(size_t) src] * depth[(size_t) src];

            float w;
            if (o.wave == waveNoise)
            {
                w = noise.next();
            }
            else
            {
                const float fb = o.feedback * o.feedback * kMaxFeedback * 0.5f * (prev1[idx] + prev2[idx]);
                w = MipTable::read (levelPtr[idx], wrap01 (phase[idx] + mod + fb));
            }

            phase[idx] += baseInc * ratioMul[idx];
            if (phase[idx] >= 1.0f || phase[idx] < 0.0f)
                phase[idx] = wrap01 (phase[idx]);

            prev2[idx] = prev1[idx];
            prev1[idx] = w;

            opOut[idx] = w * env;
            depth[idx] = lvl * lvl * kMaxModIndex * velGain * fmScale;

            if (alg.isCarrier (i))
                carrierSum += opOut[idx] * lvl;
        }

        float sig = carrierSum * carrierNorm * velGain * kVoiceGain;

        // --- Filtro ---
        cutoffSmooth += (p.cutoff - cutoffSmooth) * kLevelSmooth;
        const float octaves = p.fltEnvAmt * 7.0f * fltEnv.next() + p.lfoCutoff * 4.0f * lfoV + keyOct;
        filter.setMode (p.fltMode);
        filter.setParameters (cutoffSmooth * std::exp2 (octaves), p.reso, p.drive);
        sig = filter.process (sig);

        // --- Tremolo / AM / modulacion en anillo ---
        sig *= 1.0f - p.lfoAmp * 0.5f * (1.0f - lfoV);

        outL[startSample + s] += sig;
        if (outR != nullptr)
            outR[startSample + s] += sig;
    }

    if (carriersFinished())
    {
        resetState();
        clearCurrentNote();
    }
}
} // namespace cuarzo
