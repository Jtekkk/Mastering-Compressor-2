#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

#include "Params.h"

// ---------------------------------------------------------------------------
// Factory presets: front-panel starting points, not scientific targets - ride
// the knobs from here same as you would with the hardware. Values are the
// raw parameter values (choice/bool params use their index / 0-1 as a float)
// and land exactly on each parameter's detent grid.
// ---------------------------------------------------------------------------

namespace mc2presets
{
struct ParamValue
{
    const char* id;
    float value;
};

struct Preset
{
    const char* name;
    std::vector<ParamValue> values;
};

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = {
        { "Vocal Glue", {
            { ParamID::threshold, -18.0f }, { ParamID::attack, 30.0f },
            { ParamID::recovery, 1.0f },    { ParamID::mode, 0.0f },
            { ParamID::rectifier, 4.0f },   { ParamID::scEq, 0.0f },
            { ParamID::link, 1.0f },        { ParamID::output, 0.5f },
            { ParamID::eqIn, 0.0f },        { ParamID::eqLow, 0.0f }, { ParamID::eqAir, 0.0f },
        } },
        { "Mix Bus", {
            { ParamID::threshold, -16.0f }, { ParamID::attack, 45.0f },
            { ParamID::recovery, 1.0f },     { ParamID::mode, 0.0f },
            { ParamID::rectifier, 0.0f },    { ParamID::scEq, 1.0f },
            { ParamID::link, 1.0f },         { ParamID::output, 1.0f },
            { ParamID::eqIn, 1.0f },         { ParamID::eqLow, 1.0f }, { ParamID::eqAir, 1.5f },
        } },
        { "Drum Bus", {
            { ParamID::threshold, -14.0f }, { ParamID::attack, 25.0f },
            { ParamID::recovery, 0.0f },    { ParamID::mode, 0.0f },
            { ParamID::rectifier, 3.0f },   { ParamID::scEq, 2.0f },
            { ParamID::link, 1.0f },        { ParamID::output, 1.5f },
            { ParamID::eqIn, 0.0f },        { ParamID::eqLow, 0.0f }, { ParamID::eqAir, 0.0f },
        } },
        { "Master Gentle", {
            { ParamID::threshold, -10.0f }, { ParamID::attack, 60.0f },
            { ParamID::recovery, 3.0f },    { ParamID::mode, 0.0f },
            { ParamID::rectifier, 5.0f },   { ParamID::scEq, 0.0f },
            { ParamID::link, 1.0f },        { ParamID::output, 0.5f },
            { ParamID::eqIn, 1.0f },        { ParamID::eqLow, 0.5f }, { ParamID::eqAir, 1.0f },
        } },
        { "Loud Master", {
            { ParamID::input, 1.5f },       { ParamID::threshold, -6.0f },
            { ParamID::attack, 25.0f },     { ParamID::recovery, 2.0f },
            { ParamID::mode, 1.0f },        { ParamID::rectifier, 2.0f },
            { ParamID::scEq, 3.0f },        { ParamID::link, 1.0f },
            { ParamID::output, 2.0f },      { ParamID::eqIn, 1.0f },
            { ParamID::eqLow, 1.5f },       { ParamID::eqAir, 2.0f },
        } },
    };
    return presets;
}

inline void apply (juce::AudioProcessorValueTreeState& apvts, const Preset& preset)
{
    for (auto& pv : preset.values)
        if (auto* param = apvts.getParameter (pv.id))
            param->setValueNotifyingHost (param->convertTo0to1 (pv.value));
}
} // namespace mc2presets
