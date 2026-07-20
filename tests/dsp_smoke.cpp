// Headless verification of the MC-2 DSP core. No JUCE, no audio hardware:
// drives mc2::MC2Engine with synthetic programme and checks the behaviour
// printed on the front panel actually holds.

#include "DSP/MC2Engine.h"
#include "DSP/Metering.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
constexpr double kFs = 88200.0; // engine rate (2x oversampled 44.1k)
int failures = 0;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (cond) { std::printf ("  ok   : " __VA_ARGS__); std::printf ("\n"); } \
        else { std::printf ("  FAIL : " __VA_ARGS__); std::printf ("\n"); ++failures; } \
    } while (0)

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (int n) : l ((size_t) n, 0.0f), r ((size_t) n, 0.0f) {}
    int size() const { return (int) l.size(); }
};

Stereo sine (double freq, float peakDB, double seconds, double fs = kFs)
{
    Stereo s ((int) (seconds * fs));
    const float a = std::pow (10.0f, peakDB / 20.0f);
    for (int i = 0; i < s.size(); ++i)
        s.l[(size_t) i] = s.r[(size_t) i] =
            a * (float) std::sin (2.0 * 3.14159265358979 * freq * i / fs);
    return s;
}

void run (mc2::MC2Engine& e, Stereo& s, int blockSize = 512)
{
    for (int pos = 0; pos < s.size(); pos += blockSize)
    {
        const int n = std::min (blockSize, s.size() - pos);
        float* chans[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (chans, 2, n);
    }
}

float peakOfTail (const std::vector<float>& v, double tailSeconds)
{
    const int n = (int) v.size();
    const int start = std::max (0, n - (int) (tailSeconds * kFs));
    float p = 0.0f;
    for (int i = start; i < n; ++i)
        p = std::max (p, std::fabs (v[(size_t) i]));
    return p;
}

bool allFinite (const Stereo& s)
{
    for (int i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.l[(size_t) i]) || ! std::isfinite (s.r[(size_t) i]))
            return false;
    return true;
}

// Goertzel magnitude of one bin.
float goertzel (const std::vector<float>& v, int start, int len, double freq)
{
    const double w = 2.0 * 3.14159265358979 * freq / kFs;
    const double c = 2.0 * std::cos (w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < len; ++i)
    {
        s0 = v[(size_t) (start + i)] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return (float) (2.0 * std::sqrt (re * re + im * im) / len);
}

mc2::MC2Engine makeEngine (const mc2::EngineParams& p)
{
    mc2::MC2Engine e;
    e.prepare (kFs, 1024);
    e.setParams (p);
    return e;
}

// Steady-state output peak (dB) for a sine at the given input peak (dB).
float steadyOutDB (mc2::EngineParams p, float inDB, double seconds = 3.0)
{
    auto e = makeEngine (p);
    auto s = sine (1000.0, inDB, seconds);
    run (e, s);
    return 20.0f * std::log10 (peakOfTail (s.l, 0.25) + 1e-12f);
}

float grAfter (mc2::MC2Engine& e, Stereo& s) { run (e, s); return e.getGainReductionDB (0); }
} // namespace

int main()
{
    mc2::EngineParams base;
    base.thresholdDB = -20.0f;
    base.attackMs = 25.0f;
    base.recoveryIdx = 0;

    std::printf ("MC-2 DSP smoke test @ %.0f Hz engine rate\n", kFs);

    // ------------------------------------------------------------ silence --
    {
        auto e = makeEngine (base);
        Stereo s ((int) kFs / 2);
        run (e, s);
        CHECK (allFinite (s) && peakOfTail (s.l, 0.1) < 1e-5f && e.getGainReductionDB (0) < 0.01f,
               "silence in -> silence out, no GR");
    }

    // ----------------------------------------------------- compress ratio --
    {
        auto p = base;
        p.limitMode = false;
        const float o1 = steadyOutDB (p, -14.0f);
        const float o2 = steadyOutDB (p, -8.0f);
        const float ratio = 6.0f / std::max (0.1f, o2 - o1);
        std::printf ("  info : COMPRESS dIn=6 dB -> dOut=%.2f dB (ratio %.2f:1)\n",
                     o2 - o1, ratio);
        CHECK (ratio > 1.25f && ratio < 1.9f, "COMPRESS mode ratio ~1.5:1");
    }

    // -------------------------------------------------------- limit ratio --
    {
        auto p = base;
        p.limitMode = true;
        const float o1 = steadyOutDB (p, -12.0f);
        const float o2 = steadyOutDB (p, -4.0f);
        const float ratio = 8.0f / std::max (0.05f, o2 - o1);
        std::printf ("  info : LIMIT dIn=8 dB -> dOut=%.2f dB (ratio %.1f:1)\n",
                     o2 - o1, ratio);
        CHECK (ratio >= 3.0f, "LIMIT mode ratio >= 4:1 region");

        mc2::EngineParams noComp;
        noComp.thresholdDB = 0.0f;
        CHECK (steadyOutDB (p, -4.0f) < steadyOutDB (noComp, -4.0f) - 6.0f,
               "LIMIT mode clamps a hot signal hard");
    }

    // ------------------------------------------------------- attack range --
    {
        auto measureAttack = [&] (float atkMs) {
            auto p = base;
            p.attackMs = atkMs;
            auto e = makeEngine (p);
            auto s = sine (1000.0, -6.0f, 1.5);
            // find the time at which GR reaches 63% of its final value
            float finalGr = 0.0f;
            {
                auto e2 = makeEngine (p);
                auto s2 = sine (1000.0, -6.0f, 3.0);
                finalGr = grAfter (e2, s2);
            }
            int block = 64, t63 = -1;
            for (int pos = 0; pos < s.size(); pos += block)
            {
                const int n = std::min (block, s.size() - pos);
                float* chans[2] = { s.l.data() + pos, s.r.data() + pos };
                e.process (chans, 2, n);
                if (t63 < 0 && e.getGainReductionDB (0) >= 0.63f * finalGr)
                {
                    t63 = pos + n;
                    break;
                }
            }
            return t63 < 0 ? 1.0e9 : 1000.0 * t63 / kFs;
        };

        const double t25 = measureAttack (25.0f);
        const double t70 = measureAttack (70.0f);
        std::printf ("  info : attack-to-63%% GR: 25ms setting -> %.1f ms, 70ms -> %.1f ms\n",
                     t25, t70);
        CHECK (t25 > 5.0 && t25 < 120.0, "25 ms attack lands in a plausible window");
        CHECK (t70 > 1.5 * t25, "70 ms attack is distinctly slower than 25 ms");
    }

    // ------------------------------------------------------ recovery range --
    {
        auto measureRelease = [&] (int idx) {
            auto p = base;
            p.recoveryIdx = idx;
            auto e = makeEngine (p);
            auto burst = sine (1000.0, -6.0f, 2.0);
            run (e, burst);
            const float gr0 = e.getGainReductionDB (0);
            Stereo quiet ((int) (12.0 * kFs));
            int block = 256, t37 = -1;
            for (int pos = 0; pos < quiet.size(); pos += block)
            {
                const int n = std::min (block, quiet.size() - pos);
                float* chans[2] = { quiet.l.data() + pos, quiet.r.data() + pos };
                e.process (chans, 2, n);
                if (t37 < 0 && e.getGainReductionDB (0) <= 0.37f * gr0)
                {
                    t37 = pos + n;
                    break;
                }
            }
            return t37 < 0 ? 1.0e9 : (double) t37 / kFs;
        };

        const double rFast = measureRelease (0);   // 0.2 s
        const double rMid  = measureRelease (2);   // 0.6 s
        const double rSlow = measureRelease (4);   // 8 s
        std::printf ("  info : recovery-to-37%%: 0.2s -> %.2f s, 0.6s -> %.2f s, 8s -> %.2f s\n",
                     rFast, rMid, rSlow);
        CHECK (rFast > 0.02 && rFast < 0.8, "0.2 s recovery in range");
        CHECK (rMid > rFast, "0.6 s slower than 0.2 s");
        CHECK (rSlow > 4.0 * rMid, "8 s recovery much slower than 0.6 s");
    }

    // ------------------------------------------------- six rectifiers work --
    {
        bool allEngage = true, finite = true;
        float grs[6] = {};
        for (int rIdx = 0; rIdx < 6; ++rIdx)
        {
            auto p = base;
            p.rectifierIdx = rIdx;
            auto e = makeEngine (p);
            auto s = sine (1000.0, -6.0f, 2.5);
            grs[rIdx] = grAfter (e, s);
            allEngage = allEngage && grs[rIdx] > 1.0f;
            finite = finite && allFinite (s);
        }
        std::printf ("  info : GR by rectifier: FW %.1f, HW %.1f, Ge %.1f, Si %.1f, Opto %.1f, RMS %.1f dB\n",
                     grs[0], grs[1], grs[2], grs[3], grs[4], grs[5]);
        CHECK (allEngage && finite, "all six rectifier circuits engage and stay finite");

        bool differ = false;
        for (int a = 0; a < 6 && ! differ; ++a)
            for (int b = a + 1; b < 6 && ! differ; ++b)
                differ = std::fabs (grs[a] - grs[b]) > 0.15f;
        CHECK (differ, "rectifier palette produces distinct behaviours");
    }

    // ------------------------------------------------- sidechain EQ curves --
    {
        auto grAtCurve = [&] (int curve, double freq) {
            auto p = base;
            p.scEqIdx = curve;
            auto e = makeEngine (p);
            auto s = sine (freq, -6.0f, 2.5);
            return grAfter (e, s);
        };

        const float flat50 = grAtCurve (0, 50.0);
        const float hp50   = grAtCurve (1, 50.0);
        const float flat8k = grAtCurve (0, 8000.0);
        const float lift8k = grAtCurve (3, 8000.0);
        std::printf ("  info : 50 Hz GR flat %.1f vs HP100 %.1f | 8 kHz GR flat %.1f vs HF-LIFT %.1f\n",
                     flat50, hp50, flat8k, lift8k);
        CHECK (hp50 < flat50 - 2.0f, "HP 100 curve stops bass driving the compressor");
        CHECK (lift8k > flat8k + 1.0f, "HF LIFT curve leans on the top end");
    }

    // ------------------------------------------------------- stereo link --
    {
        auto p = base;
        p.stereoLink = true;
        auto e = makeEngine (p);
        auto s = sine (1000.0, -6.0f, 2.0);
        for (auto& v : s.r) v = 0.0f;                  // hard-left programme
        run (e, s);
        const float linkedDiff = std::fabs (e.getGainReductionDB (0) - e.getGainReductionDB (1));

        p.stereoLink = false;
        auto e2 = makeEngine (p);
        auto s2 = sine (1000.0, -6.0f, 2.0);
        for (auto& v : s2.r) v = 0.0f;
        run (e2, s2);
        const float dualL = e2.getGainReductionDB (0);
        const float dualR = e2.getGainReductionDB (1);

        std::printf ("  info : linked L/R GR diff %.2f dB | unlinked L %.1f R %.1f dB\n",
                     linkedDiff, dualL, dualR);
        CHECK (linkedDiff < 0.1f, "stereo link locks both control voltages");
        CHECK (dualL > dualR + 2.0f, "unlinked channels compress independently");
    }

    // ------------------------------------------------------- passive EQ --
    {
        mc2::EngineParams p;                       // threshold default, no GR at -30
        p.thresholdDB = 0.0f;
        p.eqIn = true;
        p.eqLowDB = 6.0f;
        p.eqAirDB = 6.0f;

        auto outAt = [&] (double freq, bool eqIn) {
            auto pp = p;
            pp.eqIn = eqIn;
            auto e = makeEngine (pp);
            auto s = sine (freq, -30.0f, 1.0);
            run (e, s);
            return 20.0f * std::log10 (peakOfTail (s.l, 0.2) + 1e-12f);
        };

        const float lowBoost = outAt (60.0, true)    - outAt (60.0, false);
        const float airBoost = outAt (16000.0, true) - outAt (16000.0, false);
        const float midShift = outAt (1000.0, true)  - outAt (1000.0, false);
        std::printf ("  info : passive EQ: +%.1f dB @60 Hz, +%.1f dB @16 kHz, %+.2f dB @1 kHz\n",
                     lowBoost, airBoost, midShift);
        CHECK (lowBoost > 3.5f && airBoost > 3.5f, "passive EQ shelves boost as labelled");
        CHECK (std::fabs (midShift) < 1.0f, "passive EQ leaves the midrange alone");
    }

    // ------------------------------------------- tube colour, sane levels --
    {
        mc2::EngineParams p;
        p.thresholdDB = 0.0f;                      // no compression
        auto e = makeEngine (p);
        auto s = sine (1000.0, -12.0f, 1.5);
        run (e, s);
        const int start = s.size() - (int) (0.5 * kFs);
        const int len = (int) (0.4 * kFs);
        const float f1 = goertzel (s.l, start, len, 1000.0);
        const float h2 = goertzel (s.l, start, len, 2000.0);
        const float h3 = goertzel (s.l, start, len, 3000.0);
        const float thd = std::sqrt (h2 * h2 + h3 * h3) / std::max (1e-9f, f1) * 100.0f;
        std::printf ("  info : twin-tube colour at -12 dBFS, no GR: THD %.3f%%\n", thd);
        CHECK (thd > 0.005f && thd < 3.0f, "tube stages add gentle, musical harmonics");
        CHECK (allFinite (s), "output finite");

        // GR-dependent bloom: harmonics grow when the mu stage works
        auto p2 = base;
        p2.limitMode = true;
        auto e2 = makeEngine (p2);
        auto s2 = sine (1000.0, -6.0f, 3.0);
        run (e2, s2);
        const float f1c = goertzel (s2.l, s2.size() - len, len, 1000.0);
        const float h2c = goertzel (s2.l, s2.size() - len, len, 2000.0);
        std::printf ("  info : 2nd harmonic, idle %.4f%% vs %.1f dB GR %.4f%%\n",
                     100.0f * h2 / f1, e2.getGainReductionDB (0), 100.0f * h2c / f1c);
        CHECK (h2c / f1c > h2 / f1, "2nd harmonic blooms with gain reduction (vari-mu bias shift)");
    }

    // ------------------------------------------------- frequency response sweep --
    {
        mc2::EngineParams p;
        p.thresholdDB = 0.0f; // no GR - measure the passive path on its own

        std::printf ("  info : frequency response sweep (no GR, EQ out), deviation from 20 Hz:\n");
        const double freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 15000, 20000 };
        constexpr float inDB = -20.0f;
        bool inBand = true;
        for (double f : freqs)
        {
            auto e = makeEngine (p);
            auto s = sine (f, inDB, 0.5);
            run (e, s);
            const float outDB = 20.0f * std::log10 (peakOfTail (s.l, 0.15) + 1e-12f);
            std::printf ("    %6.0f Hz : %+.2f dB\n", f, outDB - inDB);
            if (f >= 100.0 && f <= 10000.0)
                inBand = inBand && std::fabs (outDB - inDB) < 1.0f;
        }
        CHECK (inBand, "response stays within +/-1.0 dB, 100 Hz-10 kHz, EQ out");
    }

    // ------------------------------------------------------------ THD curve --
    {
        mc2::EngineParams p;
        p.thresholdDB = 0.0f; // no GR - isolate the tube stage's own colour

        std::printf ("  info : THD vs input level (1 kHz, no GR):\n");
        const float levels[] = { -24.0f, -18.0f, -12.0f, -6.0f, -3.0f };
        float prevThd = -1.0f;
        bool grows = true;
        for (float lvl : levels)
        {
            auto e = makeEngine (p);
            auto s = sine (1000.0, lvl, 1.5);
            run (e, s);
            const int start = s.size() - (int) (0.4 * kFs);
            const int len   = (int) (0.3 * kFs);
            const float f1 = goertzel (s.l, start, len, 1000.0);
            const float h2 = goertzel (s.l, start, len, 2000.0);
            const float h3 = goertzel (s.l, start, len, 3000.0);
            const float thd = std::sqrt (h2 * h2 + h3 * h3) / std::max (1e-9f, f1) * 100.0f;
            std::printf ("    %+5.1f dBFS : THD %.3f%%\n", lvl, thd);
            if (prevThd >= 0.0f)
                grows = grows && thd >= prevThd - 0.02f; // allow tiny numerical wobble
            prevThd = thd;
        }
        CHECK (grows, "THD rises (or holds) as input level climbs toward 0 dBFS");
    }

    // ------------------------------------------------- attack/recovery tables --
    {
        auto measureAttackMs = [&] (float atkMs) {
            auto p = base;
            p.attackMs = atkMs;
            auto e = makeEngine (p);
            auto s = sine (1000.0, -6.0f, 1.5);
            float finalGr;
            {
                auto e2 = makeEngine (p);
                auto s2 = sine (1000.0, -6.0f, 3.0);
                finalGr = grAfter (e2, s2);
            }
            int block = 64, t63 = -1;
            for (int pos = 0; pos < s.size(); pos += block)
            {
                const int n = std::min (block, s.size() - pos);
                float* chans[2] = { s.l.data() + pos, s.r.data() + pos };
                e.process (chans, 2, n);
                if (t63 < 0 && e.getGainReductionDB (0) >= 0.63f * finalGr)
                {
                    t63 = pos + n;
                    break;
                }
            }
            return t63 < 0 ? 1.0e9 : 1000.0 * t63 / kFs;
        };

        std::printf ("  info : attack-to-63%% GR across the full 25-70 ms range:\n");
        double prevAtk = 0.0;
        bool attackMonotonic = true;
        for (float atk = 25.0f; atk <= 70.0f; atk += 5.0f)
        {
            const double t = measureAttackMs (atk);
            std::printf ("    %4.0f ms setting -> %.1f ms\n", atk, t);
            if (atk > 25.0f)
                attackMonotonic = attackMonotonic && t > prevAtk;
            prevAtk = t;
        }
        CHECK (attackMonotonic, "attack-to-63%% GR increases monotonically across all 10 detents");

        auto measureReleaseS = [&] (int idx) {
            auto p = base;
            p.recoveryIdx = idx;
            auto e = makeEngine (p);
            auto burst = sine (1000.0, -6.0f, 2.0);
            run (e, burst);
            const float gr0 = e.getGainReductionDB (0);
            Stereo quiet ((int) (12.0 * kFs));
            int block = 256, t37 = -1;
            for (int pos = 0; pos < quiet.size(); pos += block)
            {
                const int n = std::min (block, quiet.size() - pos);
                float* chans[2] = { quiet.l.data() + pos, quiet.r.data() + pos };
                e.process (chans, 2, n);
                if (t37 < 0 && e.getGainReductionDB (0) <= 0.37f * gr0)
                {
                    t37 = pos + n;
                    break;
                }
            }
            return t37 < 0 ? 1.0e9 : (double) t37 / kFs;
        };

        const char* recoveryNames[5] = { "0.2 s", "0.4 s", "0.6 s", "4 s", "8 s" };
        std::printf ("  info : recovery-to-37%% across all 5 detents:\n");
        double prevRec = 0.0;
        bool recoveryMonotonic = true;
        for (int idx = 0; idx < 5; ++idx)
        {
            const double t = measureReleaseS (idx);
            std::printf ("    %-5s setting -> %.2f s\n", recoveryNames[idx], t);
            if (idx > 0)
                recoveryMonotonic = recoveryMonotonic && t > prevRec;
            prevRec = t;
        }
        CHECK (recoveryMonotonic, "recovery-to-37%% increases monotonically across all 5 detents");
    }

    // -------------------------------------------- stereo image preservation --
    {
        auto p = base;
        p.stereoLink = true;
        auto e = makeEngine (p);

        // A programme panned (not hard-left) toward L: R sits 6 dB under L.
        // A shared control voltage should compress both channels equally and
        // leave that balance alone while it works.
        auto s = sine (1000.0, -6.0f, 2.5);
        for (auto& v : s.r) v *= 0.5011872336f; // -6 dB relative to L

        run (e, s);
        const float outL = 20.0f * std::log10 (peakOfTail (s.l, 0.25) + 1e-12f);
        const float outR = 20.0f * std::log10 (peakOfTail (s.r, 0.25) + 1e-12f);
        const float balanceDrift = std::fabs ((outL - outR) - 6.0f);
        std::printf ("  info : panned programme (L-R input diff 6.0 dB) -> output diff %.2f dB\n",
                     outL - outR);
        CHECK (balanceDrift < 0.3f, "linked stereo bus keeps the mix balance within 0.3 dB");
    }

    // ------------------------------------------------------------- mono --
    {
        auto e = makeEngine (base);
        auto s = sine (1000.0, -6.0f, 1.0);
        float* chans[1] = { s.l.data() };
        e.process (chans, 1, s.size());
        CHECK (e.getGainReductionDB (0) > 1.0f, "mono operation works");
    }

    // --------------------------------------------------------- LUFS / true peak --
    {
        mc2::LoudnessMeter meter;
        meter.prepare (kFs, 2);
        Stereo silence ((int) (2.0 * kFs));
        float* chans[2] = { silence.l.data(), silence.r.data() };
        meter.process (chans, 2, silence.size());
        std::printf ("  info : silence -> LUFS-I %.1f, LUFS-S %.1f\n",
                     meter.getIntegratedLUFS(), meter.getShortTermLUFS());
        CHECK (meter.getIntegratedLUFS() <= -69.0f, "silence never crosses the LUFS-I absolute gate");
    }

    {
        auto lufsIFor = [&] (float peakDB) {
            mc2::LoudnessMeter meter;
            meter.prepare (kFs, 1);
            auto s = sine (1000.0, peakDB, 2.0);
            float* chans[1] = { s.l.data() };
            meter.process (chans, 1, s.size());
            return meter.getIntegratedLUFS();
        };

        const float lufsHot   = lufsIFor (0.0f);
        const float lufsQuiet = lufsIFor (-12.0f);
        std::printf ("  info : LUFS-I @0 dBFS %.2f vs @-12 dBFS %.2f (diff %.2f dB)\n",
                     lufsHot, lufsQuiet, lufsHot - lufsQuiet);
        CHECK (std::fabs ((lufsHot - lufsQuiet) - 12.0f) < 0.5f,
               "LUFS-I tracks input level 1:1 (K-weighting doesn't change with level)");
    }

    {
        mc2::LoudnessMeter meterCombined;
        meterCombined.prepare (kFs, 1);
        auto loud = sine (1000.0, 0.0f, 3.0);
        {
            float* chans[1] = { loud.l.data() };
            meterCombined.process (chans, 1, loud.size());
        }
        Stereo quiet ((int) (3.0 * kFs));
        {
            float* chans[1] = { quiet.l.data() };
            meterCombined.process (chans, 1, quiet.size());
        }
        const float lufsCombined = meterCombined.getIntegratedLUFS();

        mc2::LoudnessMeter meterLoudAlone;
        meterLoudAlone.prepare (kFs, 1);
        auto loudAlone = sine (1000.0, 0.0f, 3.0);
        float* chansAlone[1] = { loudAlone.l.data() };
        meterLoudAlone.process (chansAlone, 1, loudAlone.size());
        const float lufsLoudAlone = meterLoudAlone.getIntegratedLUFS();

        std::printf ("  info : LUFS-I loud+quiet %.2f vs loud-alone %.2f (gate should reject the quiet half)\n",
                     lufsCombined, lufsLoudAlone);
        CHECK (std::fabs (lufsCombined - lufsLoudAlone) < 0.5f,
               "relative gate excludes a quiet half instead of averaging it in");
    }

    {
        auto s = sine (997.0, 0.0f, 0.5);
        float samplePeak = 0.0f;
        for (float v : s.l) samplePeak = std::max (samplePeak, std::fabs (v));
        const float samplePeakDB = 20.0f * std::log10 (samplePeak);

        mc2::LoudnessMeter meter;
        meter.prepare (kFs, 1);
        float* chans[1] = { s.l.data() };
        meter.process (chans, 1, s.size());

        std::printf ("  info : 0 dBFS 997 Hz tone -> sample peak %.3f dB, true peak %.3f dBTP\n",
                     samplePeakDB, meter.getTruePeakDB());
        CHECK (meter.getTruePeakDB() >= samplePeakDB - 0.01f,
               "true-peak estimate is never below the plain sample peak");
    }

    {
        // A two-sample 0 dBFS pulse bracketed by silence: Catmull-Rom
        // interpolation between the two full-scale samples (0,1,1,0 as the
        // four control points) overshoots to 1.125 at the midpoint - a real
        // inter-sample over a plain sample-peak reading (0.0 dB) would miss.
        std::vector<float> pulse (20, 0.0f);
        pulse[10] = 1.0f;
        pulse[11] = 1.0f;

        mc2::LoudnessMeter meter;
        meter.prepare (kFs, 1);
        float* chans[1] = { pulse.data() };
        meter.process (chans, 1, (int) pulse.size());

        std::printf ("  info : two-sample 0 dBFS pulse -> true peak %.2f dBTP (sample peak is exactly 0.0)\n",
                     meter.getTruePeakDB());
        CHECK (meter.getTruePeakDB() > 0.5f,
               "true-peak estimator catches an inter-sample over a plain sample peak would miss");
    }

    std::printf (failures == 0 ? "\nALL CHECKS PASSED\n"
                               : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
