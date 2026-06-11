#pragma once

#include <algorithm>
#include <cmath>

namespace mc2
{

// ---------------------------------------------------------------------------
// Twin-tube signal path model.
//
// Stage A — input triode: a fixed, very gentle transfer curve. Mostly low
// 3rd harmonic, the "always on" tube sheen of the line amplifier.
//
// Stage B — the 5670 dual-triode variable-gain element. In a vari-mu design
// the tube *is* the gain control: the control voltage re-biases the grid,
// sliding the operating point along the transfer curve. Two consequences are
// modelled here:
//   * gain reduction is applied as the mu-stage gain (passed in), and
//   * the bias point shifts with gain reduction, so 2nd-harmonic content
//     blooms as the unit compresses — the classic vari-mu "thickening".
//
// Both stages are normalised for exact unity small-signal gain and zero DC
// at rest, so the static gain is set purely by the control voltage and the
// harmonic content purely by the curvature.
//
// The 120 V B+ rails give the circuit enormous headroom: the final soft
// ceiling sits at +24 dBFS and is effectively never hit by programme.
// ---------------------------------------------------------------------------

struct TubeStage
{
    // Input triode
    float driveA = 0.6f;
    float biasA  = 0.03f;

    // 5670 variable-mu triode
    float driveB    = 0.95f;
    float biasB0    = 0.04f;
    float biasPerDB = 0.008f;   // grid bias shift per dB of gain reduction

    // Biased tanh triode law, normalised to unity gain / zero DC at rest.
    static inline float shape (float x, float d, float b) noexcept
    {
        const float t0 = std::tanh (d * b);
        const float t  = std::tanh (d * (x + b));
        const float sech2 = 1.0f - t0 * t0;
        return (t - t0) / (d * sech2);
    }

    inline float process (float x, float grDB, float muGain) const noexcept
    {
        const float a = shape (x, driveA, biasA);
        const float bias = biasB0 + biasPerDB * std::min (grDB, 20.0f);
        return shape (a * muGain, driveB, bias);
    }
};

// +24 dBFS soft ceiling — the "120 volt rails".
inline float railLimit (float x) noexcept
{
    constexpr float rails = 15.8489f; // 10^(24/20)
    return rails * std::tanh (x / rails);
}

} // namespace mc2
