#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/MC2Engine.h"
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

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // Meter feed for the editor (lock-free).
    std::atomic<float> meterGrDB[2]  { 0.0f, 0.0f };
    std::atomic<float> meterOutRms[2] { 0.0f, 0.0f };

private:
    void updateEngineParams();

    mc2::MC2Engine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

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
    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MC2AudioProcessor)
};
