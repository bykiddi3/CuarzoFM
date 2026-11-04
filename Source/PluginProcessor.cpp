#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace cuarzo;

namespace
{
    constexpr int kNumVoices = 16;
    inline float rd (std::atomic<float>* p) noexcept { return p->load (std::memory_order_relaxed); }

    // Limitador suave de seguridad: transparente por debajo de 0.8, satura con suavidad por encima
    inline float softLimit (float x) noexcept
    {
        constexpr float knee = 0.8f;
        const float ax = std::abs (x);
        if (ax <= knee) return x;
        const float over = (ax - knee) / (1.0f - knee);
        return std::copysign (knee + (1.0f - knee) * fastTanh (over), x);
    }
}

CuarzoProcessor::CuarzoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CuarzoFM", createLayout())
{
    cacheParameterPointers();

    // Armonicos por defecto: espectro de diente de sierra (1/n)
    for (auto& h : harmonics)
        for (int n = 0; n < kNumHarmonics; ++n)
            h[(size_t) n] = 1.0f / (float) (n + 1);

    drawnTables = std::make_unique<std::array<std::array<MipTable, 2>, kNumOscs>>();
    for (int i = 0; i < kNumOscs; ++i)
    {
        activeDrawn[(size_t) i].store (0);
        (*drawnTables)[(size_t) i][0].buildFromHarmonics (harmonics[(size_t) i].data(), kNumHarmonics);
    }

    (void) ClassicWaves::get(); // construye las tablas compartidas fuera del hilo de audio

    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new CuarzoVoice (voiceParams, (uint32_t) (i + 1)));
    synth.addSound (new CuarzoSound());
    synth.setNoteStealingEnabled (true);
}

void CuarzoProcessor::cacheParameterPointers()
{
    auto get = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };
    auto env = [&get] (const juce::String& prefix)
    {
        return EnvPtrs { get (ID::env (prefix, "att")), get (ID::env (prefix, "dec")), get (ID::env (prefix, "sus")), get (ID::env (prefix, "rel")) };
    };

    ptr.algorithm = get (ID::algorithm);
    ptr.master    = get (ID::master);
    ptr.velSens   = get (ID::velSens);
    ptr.bendRange = get (ID::bendRange);

    for (int i = 0; i < kNumOscs; ++i)
        ptr.osc[(size_t) i] = OscPtrs { get (ID::osc (i, "wave")), get (ID::osc (i, "ratio")), get (ID::osc (i, "fine")),
                                        get (ID::osc (i, "level")), get (ID::osc (i, "fb")), env (ID::envOsc (i)) };

    ptr.fltMode   = get (ID::fltMode);
    ptr.cutoff    = get (ID::fltCutoff);
    ptr.reso      = get (ID::fltReso);
    ptr.drive     = get (ID::fltDrive);
    ptr.fltEnvAmt = get (ID::fltEnv);
    ptr.fltKey    = get (ID::fltKey);
    ptr.fltEnv    = env (ID::envFilter);

    ptr.pitAmt = get (ID::pitAmount);
    ptr.pitEnv = env (ID::envPitch);

    ptr.lfoWave   = get (ID::lfoWave);
    ptr.lfoRate   = get (ID::lfoRate);
    ptr.lfoKey    = get (ID::lfoKey);
    ptr.lfoRetrig = get (ID::lfoRetrig);
    ptr.lfoPitch  = get (ID::lfoPitch);
    ptr.lfoCutoff = get (ID::lfoCutoff);
    ptr.lfoAmp    = get (ID::lfoAmp);
    ptr.lfoFm     = get (ID::lfoFm);
    ptr.lfoEnv    = env (ID::envLfo);

    ptr.dstType  = get (ID::dstType);
    ptr.dstDrive = get (ID::dstDrive);
    ptr.dstMix   = get (ID::dstMix);
}

void CuarzoProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    distortion.prepare (sampleRate);

    masterGain.reset (sampleRate, 0.02);
    distMix.reset (sampleRate, 0.02);
    distDrive.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (rd (ptr.master)));
    distMix.setCurrentAndTargetValue (rd (ptr.dstMix));
    distDrive.setCurrentAndTargetValue (rd (ptr.dstDrive));
}

bool CuarzoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CuarzoProcessor::snapshotParameters (int numSamples)
{
    auto& vp = voiceParams;
    auto envOf = [] (const EnvPtrs& e) { return EnvParams { rd (e.a), rd (e.d), rd (e.s), rd (e.r) }; };

    vp.algorithm = juce::jlimit (0, kNumAlgorithms - 1, (int) rd (ptr.algorithm) - 1);

    for (int i = 0; i < kNumOscs; ++i)
    {
        auto& o = vp.osc[(size_t) i];
        const auto& op = ptr.osc[(size_t) i];
        o.wave      = juce::jlimit (0, (int) numWaves - 1, (int) rd (op.wave));
        o.ratio     = kRatios[(size_t) juce::jlimit (0, (int) kRatios.size() - 1, (int) rd (op.ratio))];
        o.fineCents = rd (op.fine);
        o.level     = rd (op.level);
        o.feedback  = rd (op.fb);
        o.env       = envOf (op.env);
        o.drawn     = &(*drawnTables)[(size_t) i][(size_t) activeDrawn[(size_t) i].load (std::memory_order_acquire)];
    }

    vp.fltMode   = (int) rd (ptr.fltMode);
    vp.cutoff    = rd (ptr.cutoff);
    vp.reso      = rd (ptr.reso);
    vp.drive     = rd (ptr.drive);
    vp.fltEnvAmt = rd (ptr.fltEnvAmt);
    vp.fltKey    = rd (ptr.fltKey);
    vp.fltEnv    = envOf (ptr.fltEnv);

    vp.pitchAmt = rd (ptr.pitAmt);
    vp.pitchEnv = envOf (ptr.pitEnv);

    vp.lfoWave   = (int) rd (ptr.lfoWave);
    vp.lfoRate   = rd (ptr.lfoRate);
    vp.lfoKey    = rd (ptr.lfoKey) > 0.5f;
    vp.lfoRetrig = rd (ptr.lfoRetrig) > 0.5f;
    vp.lfoPitch  = rd (ptr.lfoPitch);
    vp.lfoCutoff = rd (ptr.lfoCutoff);
    vp.lfoAmp    = rd (ptr.lfoAmp);
    vp.lfoFm     = rd (ptr.lfoFm);
    vp.lfoEnv    = envOf (ptr.lfoEnv);

    // Fase libre compartida para cuando el LFO no se reinicia con cada nota
    vp.lfoFreePhase += (float) (vp.lfoRate * numSamples / currentSampleRate);
    vp.lfoFreePhase -= std::floor (vp.lfoFreePhase);

    vp.velSens   = rd (ptr.velSens);
    vp.bendRange = (int) rd (ptr.bendRange);
}

void CuarzoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    snapshotParameters (numSamples);
    synth.renderNextBlock (buffer, midi, 0, numSamples);

    // --- Distorsion global + volumen ---
    const int distType = (int) rd (ptr.dstType);
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (rd (ptr.master), -48.0f));
    distMix.setTargetValue (rd (ptr.dstMix));
    distDrive.setTargetValue (rd (ptr.dstDrive));

    auto* l = buffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        const float dry = l[i];
        const float mix = distMix.getNextValue();
        const float wet = distortion.process (dry, distType, distDrive.getNextValue());
        const float y = distType == Distortion::off ? dry : dry + mix * (wet - dry);
        l[i] = softLimit (y * masterGain.getNextValue());
    }
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

// --------------------------- Armonicos ---------------------------
void CuarzoProcessor::setHarmonics (int osc, const Harmonics& values)
{
    if (osc < 0 || osc >= kNumOscs) return;
    harmonics[(size_t) osc] = values;
    rebuildDrawnTable (osc);
    ++harmonicsVersion;
}

void CuarzoProcessor::rebuildDrawnTable (int osc)
{
    // Doble buffer: se escribe la tabla inactiva y luego se publica de forma atomica
    const int next = 1 - activeDrawn[(size_t) osc].load();
    (*drawnTables)[(size_t) osc][(size_t) next].buildFromHarmonics (harmonics[(size_t) osc].data(), kNumHarmonics);
    activeDrawn[(size_t) osc].store (next, std::memory_order_release);
}

// --------------------------- Programas ---------------------------
int CuarzoProcessor::getNumPrograms()                         { return presets::count(); }
const juce::String CuarzoProcessor::getProgramName (int index) { return presets::name (index); }

void CuarzoProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= presets::count()) return;
    currentProgram = index;
    presets::apply (index, *this);
}

// --------------------------- Estado ---------------------------
void CuarzoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("HARMONICS"); old.isValid())
        state.removeChild (old, nullptr);

    juce::ValueTree h ("HARMONICS");
    for (int i = 0; i < kNumOscs; ++i)
    {
        juce::StringArray vals;
        for (auto v : harmonics[(size_t) i]) vals.add (juce::String (v, 4));
        h.setProperty ("osc" + oscLetter (i), vals.joinIntoString (","), nullptr);
    }
    state.appendChild (h, nullptr);
    state.setProperty ("program", currentProgram, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CuarzoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = (int) tree.getProperty ("program", 0);

    if (auto h = tree.getChildWithName ("HARMONICS"); h.isValid())
    {
        for (int i = 0; i < kNumOscs; ++i)
        {
            auto vals = juce::StringArray::fromTokens (h.getProperty ("osc" + oscLetter (i)).toString(), ",", "");
            Harmonics hv {};
            for (int n = 0; n < kNumHarmonics && n < vals.size(); ++n)
                hv[(size_t) n] = juce::jlimit (0.0f, 1.0f, vals[n].getFloatValue());
            setHarmonics (i, hv);
        }
        tree.removeChild (h, nullptr);
    }
    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* CuarzoProcessor::createEditor() { return new CuarzoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CuarzoProcessor(); }
