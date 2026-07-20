#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// Every control is a switch or a detented knob: floats use coarse steps so
// settings are exactly repeatable, everything else is a discrete choice.
// ---------------------------------------------------------------------------

namespace ParamID
{
    inline constexpr const char* input     = "input";
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* recovery  = "recovery";
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* rectifier = "rectifier";
    inline constexpr const char* scEq      = "sc_eq";
    inline constexpr const char* link      = "link";
    inline constexpr const char* output    = "output";
    inline constexpr const char* eqIn      = "eq_in";
    inline constexpr const char* eqLow     = "eq_low";
    inline constexpr const char* eqAir     = "eq_air";
    inline constexpr const char* bypass    = "bypass";
    inline constexpr const char* meterMode = "meter_mode";
    inline constexpr const char* calL      = "cal_l";
    inline constexpr const char* calR      = "cal_r";
    inline constexpr const char* oversampling = "oversampling";
}

namespace ParamText
{
    inline const juce::StringArray recoverySteps { "0.2 s", "0.4 s", "0.6 s", "4 s", "8 s" };
    inline const juce::StringArray modes         { "COMPRESS", "LIMIT" };
    inline const juce::StringArray rectifiers    { "TUBE FW", "TUBE HW", "GERMANIUM",
                                                   "SILICON", "OPTO", "RMS" };
    inline const juce::StringArray scCurves      { "FLAT", "HP 100", "HP200+PRES", "HF LIFT" };
    inline const juce::StringArray meterModes    { "GR", "OUTPUT" };
    inline const juce::StringArray oversamplingChoices { "1x", "2x", "4x", "8x" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using FloatParam  = AudioParameterFloat;
    using ChoiceParam = AudioParameterChoice;
    using BoolParam   = AudioParameterBool;

    auto dbText = [] (float v, int) {
        return String (v >= 0.0f ? "+" : "") + String (v, 1) + " dB";
    };
    auto msText = [] (float v, int) {
        return String (roundToInt (v)) + " ms";
    };

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::input, 1 }, "Input",
        NormalisableRange<float> (-12.0f, 12.0f, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::threshold, 1 }, "Threshold",
        NormalisableRange<float> (-40.0f, 0.0f, 0.5f), -12.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::attack, 1 }, "Attack",
        NormalisableRange<float> (25.0f, 70.0f, 5.0f), 50.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

    layout.add (std::make_unique<ChoiceParam> (
        ParameterID { ParamID::recovery, 1 }, "Recovery", ParamText::recoverySteps, 1));

    layout.add (std::make_unique<ChoiceParam> (
        ParameterID { ParamID::mode, 1 }, "Mode", ParamText::modes, 0));

    layout.add (std::make_unique<ChoiceParam> (
        ParameterID { ParamID::rectifier, 1 }, "Rectifier", ParamText::rectifiers, 0));

    layout.add (std::make_unique<ChoiceParam> (
        ParameterID { ParamID::scEq, 1 }, "Sidechain EQ", ParamText::scCurves, 0));

    layout.add (std::make_unique<BoolParam> (
        ParameterID { ParamID::link, 1 }, "Stereo Link", true));

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::output, 1 }, "Output",
        NormalisableRange<float> (-12.0f, 12.0f, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<BoolParam> (
        ParameterID { ParamID::eqIn, 1 }, "EQ In", false));

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::eqLow, 1 }, "EQ Low 90 Hz",
        NormalisableRange<float> (0.0f, 6.0f, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::eqAir, 1 }, "EQ Air 12 kHz",
        NormalisableRange<float> (0.0f, 6.0f, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<BoolParam> (
        ParameterID { ParamID::bypass, 1 }, "Hard Bypass", false));

    layout.add (std::make_unique<ChoiceParam> (
        ParameterID { ParamID::meterMode, 1 }, "Meter", ParamText::meterModes, 0));

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::calL, 1 }, "Meter Cal L",
        NormalisableRange<float> (-3.0f, 3.0f, 0.25f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<FloatParam> (
        ParameterID { ParamID::calR, 1 }, "Meter Cal R",
        NormalisableRange<float> (-3.0f, 3.0f, 0.25f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    // Structural, not a musical control: changing it re-prepares the engine
    // at a new internal rate, so it's excluded from host automation lanes.
    layout.add (std::make_unique<ChoiceParam> (
        ParameterID { ParamID::oversampling, 1 }, "Oversampling", ParamText::oversamplingChoices, 1,
        AudioParameterChoiceAttributes().withAutomatable (false)));

    return layout;
}
