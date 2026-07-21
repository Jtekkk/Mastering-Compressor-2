#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GUI/GrHistoryGraph.h"
#include "GUI/MC2LookAndFeel.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/VUMeter.h"
#include "PluginProcessor.h"

class MC2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit MC2AudioProcessorEditor (MC2AudioProcessor&);
    ~MC2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;

    void setupKnob (juce::Slider& s, const char* paramID, int tickCount);
    void setupSwitch (juce::Slider& s, const char* paramID, const juce::StringArray& labels);
    void setupToggle (juce::ToggleButton& b, const char* paramID,
                      const char* onLabel, const char* offLabel);

    juce::Rectangle<int> mainStation (int index) const;
    juce::Rectangle<int> lowerStation (int index) const;

    MC2AudioProcessor& proc;
    MC2LookAndFeel lnf;

    VUMeter meterL { "LEFT" }, meterR { "RIGHT" };
    TubeWindow tubeWindow;
    GrHistoryGraph grHistory;
    SpectrumAnalyzer spectrum;

    juce::Slider inputKnob, thresholdKnob, attackKnob, outputKnob;
    juce::Slider recoverySwitch, rectifierSwitch, scEqSwitch;
    juce::Slider eqLowKnob, eqAirKnob, calLKnob, calRKnob;
    juce::ToggleButton modeToggle, linkToggle, eqInToggle, meterToggle, bypassToggle;

    juce::ComboBox presetBox;
    juce::TextButton undoButton { "UNDO" }, redoButton { "REDO" };
    int undoTransactionCountdown = 0;

    juce::Label lufsILabel, lufsSLabel, truePeakLabel;

    juce::ComboBox oversamplingBox;
    std::unique_ptr<ComboBoxAttachment> oversamplingAttachment;

    juce::TextButton msToggle;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    std::atomic<float>* pMeterMode = nullptr;
    std::atomic<float>* pCalL = nullptr;
    std::atomic<float>* pCalR = nullptr;
    std::atomic<float>* pBypass = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MC2AudioProcessorEditor)
};
