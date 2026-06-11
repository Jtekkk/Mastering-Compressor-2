#pragma once

#include <cmath>

namespace mc2
{

// ---------------------------------------------------------------------------
// Six sidechain rectifier circuits — the heart of the "wide sonic palette".
//
// In the hardware, the rectifier that derives the control voltage from the
// sidechain signal shapes the entire feel of the compressor: how fast it
// grabs, how it lets go, and how its detection law tracks programme material.
//
//   0  TUBE FW    — twin-diode tube full-wave. Slightly compressive detection
//                   law (the tube rectifier "sags" on peaks), the classic
//                   gentle vari-mu feel.
//   1  TUBE HW    — single-plate tube half-wave. Reads only positive half
//                   cycles; asymmetric programme pumps it differently, a
//                   looser, more vintage behaviour.
//   2  GERMANIUM  — Ge diode bridge. Soft conduction knee, comes on early and
//                   smoothly; warm and forgiving.
//   3  SILICON    — Si precision bridge. Hard knee, fastest and most accurate
//                   tracking; tight, modern.
//   4  OPTO       — rectifier feeding a photo-cell. Adds a slow integrating
//                   stage with programme-dependent memory; smooth and creamy.
//   5  RMS        — square-law bridge (true RMS). Reads power rather than
//                   peaks; dense, unflappable, great on full mixes.
// ---------------------------------------------------------------------------

enum class Rectifier
{
    TubeFW = 0,
    TubeHW,
    Germanium,
    Silicon,
    Opto,
    RMS,
    Count
};

struct RectifierTraits
{
    float attackScale;   // multiplies the front-panel attack time
    float releaseScale;  // multiplies the selected recovery time
    bool  squaredLaw;    // detector state lives in the power domain
    bool  optoStage;     // extra slow integrating stage after the detector
};

inline RectifierTraits rectifierTraits (Rectifier r) noexcept
{
    switch (r)
    {
        case Rectifier::TubeFW:    return { 1.00f, 1.00f, false, false };
        case Rectifier::TubeHW:    return { 1.12f, 1.08f, false, false };
        case Rectifier::Germanium: return { 0.90f, 0.85f, false, false };
        case Rectifier::Silicon:   return { 0.75f, 0.80f, false, false };
        case Rectifier::Opto:      return { 1.30f, 1.60f, false, true  };
        case Rectifier::RMS:       return { 1.00f, 1.00f, true,  false };
        default:                   return { 1.00f, 1.00f, false, false };
    }
}

// Instantaneous detection law, before ballistic smoothing.
inline float rectify (Rectifier r, float s) noexcept
{
    switch (r)
    {
        case Rectifier::TubeFW:
        {
            // Full-wave with tube sag: under-reads large peaks a touch,
            // which eases the effective ratio at heavy drive.
            const float v = std::fabs (s);
            return v / (1.0f + 0.22f * v);
        }
        case Rectifier::TubeHW:
        {
            // Positive half-cycles only, scaled so a symmetric signal
            // reads the same average as full-wave.
            const float v = s > 0.0f ? 2.0f * s : 0.0f;
            return v / (1.0f + 0.22f * v);
        }
        case Rectifier::Germanium:
        {
            // Soft conduction knee around -32 dB; the diode starts to
            // conduct gradually instead of switching on.
            constexpr float k = 0.025f;
            const float v = std::sqrt (s * s + k * k) - k;
            return v;
        }
        case Rectifier::Silicon:
            return std::fabs (s);

        case Rectifier::Opto:
            return std::fabs (s);

        case Rectifier::RMS:
            return s * s;

        default:
            return std::fabs (s);
    }
}

} // namespace mc2
