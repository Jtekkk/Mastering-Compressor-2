#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

#include "DSP/MC2Engine.h"
#include "DSP/Metering.h"
#include "Params.h"

class MC2AudioProcessor : public juce::AudioProcessor
{
public:
    MC2AudioProcessor();
    ~MC2AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return JucePlugin_Name; }
    bool acceptsMidi() const override                     { return false; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override                      { return currentProgramIndex; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // Declared before apvts: JUCE constructs members in declaration order, and
    // the APVTS needs the manager to already exist to wire undo/redo through it.
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // Meter feed for the editor (lock-free).
    std::atomic<float> meterGrDB[2]  { 0.0f, 0.0f };
    std::atomic<float> meterOutRms[2] { 0.0f, 0.0f };
    std::atomic<float> meterLufsI      { -70.0f };
    std::atomic<float> meterLufsS      { -70.0f };
    std::atomic<float> meterTruePeakDB { -100.0f };

private:
    void updateEngineParams();
    void updateLoudnessMeters (const juce::AudioBuffer<float>& buffer, int numCh, int n);
    void applyOversamplingIndex (int idx);

    mc2::MC2Engine engine;
    mc2::LoudnessMeter loudnessMeter;

    // All 4 factors (1x/2x/4x/8x) are preallocated in prepareToPlay so that
    // switching between them at runtime - which the OVERSAMPLING control
    // allows at any time, not just between host prepareToPlay calls - never
    // allocates on the audio thread; it's just a change of which index is
    // active plus a (allocation-free) MC2Engine::prepare() at the new rate.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> oversamplers;
    int activeOversamplingIdx = 1; // 2x, matching the parameter's default
    double hostSampleRate = 44100.0;
    int hostBlockSize = 512;

    // Cached raw parameter pointers (atomics owned by the APVTS).
    std::atomic<float>* pInput     = nullptr;
    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pAttack    = nullptr;
    std::atomic<float>* pRecovery  = nullptr;
    std::atomic<float>* pMode      = nullptr;
    std::atomic<float>* pRectifier = nullptr;
    std::atomic<float>* pScEq      = nullptr;
    std::atomic<float>* pLink      = nullptr;
    std::atomic<float>* pOutput    = nullptr;
    std::atomic<float>* pEqIn      = nullptr;
    std::atomic<float>* pEqLow     = nullptr;
    std::atomic<float>* pEqAir     = nullptr;
    std::atomic<float>* pOversampling = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    int currentProgramIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MC2AudioProcessor)
};
