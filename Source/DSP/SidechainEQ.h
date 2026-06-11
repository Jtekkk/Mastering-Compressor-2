#pragma once

#include "Biquad.h"

namespace mc2
{

// ---------------------------------------------------------------------------
// Four built-in sidechain EQ curves for fine-tuning the compression response.
//
//   0  FLAT       — full-bandwidth detection.
//   1  HP 100     — 12 dB/oct high-pass at 100 Hz: bass no longer drives the
//                   compressor, kick/bass stop pumping the whole mix.
//   2  HP 200 + P — high-pass at 200 Hz plus a +2 dB presence lift at 3 kHz:
//                   vocal/midrange-forward detection, keeps the low end fat.
//   3  HF LIFT    — +4 dB high shelf from 5 kHz: the detector leans on
//                   brightness and sibilance, gently de-essing the programme.
// ---------------------------------------------------------------------------

class SidechainEQ
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        apply (curve);
        reset();
    }

    void reset()
    {
        f1.reset();
        f2.reset();
    }

    void setCurve (int newCurve)
    {
        if (newCurve == curve)
            return;
        curve = newCurve;
        apply (curve);
    }

    inline float process (float x) noexcept
    {
        if (curve == 0)
            return x;
        return f2.process (f1.process (x));
    }

    static constexpr int numCurves = 4;

private:
    void apply (int c)
    {
        f1.identity();
        f2.identity();
        switch (c)
        {
            case 1: f1.highpass (fs, 100.0, 0.707); break;
            case 2: f1.highpass (fs, 200.0, 0.707);
                    f2.peak (fs, 3000.0, 2.0, 0.7);  break;
            case 3: f2.highShelf (fs, 5000.0, 4.0, 0.7); break;
            default: break;
        }
    }

    double fs = 48000.0;
    int curve = 0;
    Biquad f1, f2;
};

} // namespace mc2
