#pragma once

#include "Biquad.h"

namespace mc2
{

// ---------------------------------------------------------------------------
// "Sweet" passive-style programme EQ for final polish.
//
// Boost-only, broad, low-Q curves in the passive LC tradition:
//   LOW — up to +6 dB shelf at 90 Hz, wide and bloomy.
//   AIR — up to +6 dB shelf at 12 kHz, silky top.
//
// Sits after the compressor, before the output stage, like the passive
// network between the gain make-up stages in the hardware.
// ---------------------------------------------------------------------------

class PassiveEQ
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        update (true);
        low.reset();
        air.reset();
    }

    void setGains (float lowDB, float airDB)
    {
        if (lowDB != lowGain || airDB != airGain)
        {
            lowGain = lowDB;
            airGain = airDB;
            update (false);
        }
    }

    inline float process (float x) noexcept
    {
        return air.process (low.process (x));
    }

private:
    void update (bool force)
    {
        (void) force;
        if (lowGain > 0.001f) low.lowShelf  (fs, 90.0,    lowGain, 0.55);
        else                  low.identity();
        if (airGain > 0.001f) air.highShelf (fs, 12000.0, airGain, 0.60);
        else                  air.identity();
    }

    double fs = 48000.0;
    float lowGain = 0.0f, airGain = 0.0f;
    Biquad low, air;
};

} // namespace mc2
