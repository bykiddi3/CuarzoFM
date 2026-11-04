#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthVoice.h"

class CuarzoProcessor : public juce::AudioProcessor
{
public:
    using Harmonics = std::array<float, cuarzo::kNumHarmonics>;

    CuarzoProcessor();
    ~CuarzoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Armonicos dibujados (hilo de mensajes) ---
    const Harmonics& getHarmonics (int osc) const { return harmonics[(size_t) osc]; }
    void setHarmonics (int osc, const Harmonics& values);
    int  getHarmonicsVersion() const noexcept { return harmonicsVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void cacheParameterPointers();
    void snapshotParameters (int numSamples);
    void rebuildDrawnTable (int osc);

    struct EnvPtrs { std::atomic<float>* a; std::atomic<float>* d; std::atomic<float>* s; std::atomic<float>* r; };
    struct OscPtrs { std::atomic<float>* wave; std::atomic<float>* ratio; std::atomic<float>* fine; std::atomic<float>* level; std::atomic<float>* fb; EnvPtrs env; };
    struct
    {
        std::atomic<float>* algorithm; std::atomic<float>* master; std::atomic<float>* velSens; std::atomic<float>* bendRange;
        std::array<OscPtrs, cuarzo::kNumOscs> osc;
        std::atomic<float>* fltMode; std::atomic<float>* cutoff; std::atomic<float>* reso; std::atomic<float>* drive; std::atomic<float>* fltEnvAmt; std::atomic<float>* fltKey;
        EnvPtrs fltEnv;
        std::atomic<float>* pitAmt; EnvPtrs pitEnv;
        std::atomic<float>* lfoWave; std::atomic<float>* lfoRate; std::atomic<float>* lfoKey; std::atomic<float>* lfoRetrig;
        std::atomic<float>* lfoPitch; std::atomic<float>* lfoCutoff; std::atomic<float>* lfoAmp; std::atomic<float>* lfoFm;
        EnvPtrs lfoEnv;
        std::atomic<float>* dstType; std::atomic<float>* dstDrive; std::atomic<float>* dstMix;
    } ptr {};

    juce::Synthesiser synth;
    cuarzo::VoiceParams voiceParams;
    cuarzo::Distortion distortion;
    juce::SmoothedValue<float> masterGain { 0.5f }, distMix { 1.0f }, distDrive { 0.3f };
    double currentSampleRate = 44100.0;

    std::array<Harmonics, cuarzo::kNumOscs> harmonics {};
    std::unique_ptr<std::array<std::array<cuarzo::MipTable, 2>, cuarzo::kNumOscs>> drawnTables;
    std::array<std::atomic<int>, cuarzo::kNumOscs> activeDrawn {};
    std::atomic<int> harmonicsVersion { 0 };

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CuarzoProcessor)
};
