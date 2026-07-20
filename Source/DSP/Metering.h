#pragma once

#include "Biquad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace mc2
{

// ---------------------------------------------------------------------------
// A mixing/mastering reference loudness meter - not a certified compliance
// measurement. K-weighting uses RBJ-cookbook high-shelf + high-pass filters
// tuned to the shape of the ITU-R BS.1770 K-curve (not its exact spec
// biquad coefficients). LUFS-I follows the standard two-stage gated block
// scheme (400 ms blocks, 100 ms step i.e. 75% overlap; absolute gate at
// -70 LUFS, relative gate at -10 LU below the ungated mean). LUFS-S is a
// plain, ungated 3 s sliding window, matching how EBU R128 defines momentary
// and short-term loudness (only the *integrated* figure is gated). True peak
// uses a 4-point Catmull-Rom interpolation (4x oversample, the minimum
// factor BS.1770 Annex 2 recommends) - close to, but not identical to, the
// spec's dedicated FIR.
//
// Real-time notes: everything in process() is O(1) per sample. The gated
// block history is a fixed-size ring buffer (no allocation after prepare());
// the O(blockCount) gating recompute only runs once per 100 ms step, and
// history is capped at kMaxBlocks (20 minutes) to bound that cost.
// ---------------------------------------------------------------------------
class LoudnessMeter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        numCh = std::max (1, numChannels);

        shelf.assign ((size_t) numCh, Biquad{});
        hpf.assign ((size_t) numCh, Biquad{});
        for (int c = 0; c < numCh; ++c)
        {
            shelf[(size_t) c].highShelf (fs, 1500.0, 4.0, 1.0 / std::sqrt (2.0));
            hpf[(size_t) c].highpass (fs, 38.0, 0.5);
        }

        shortWindow = std::max (1, (int) std::lround (fs * 3.0));
        gateWindow  = std::max (1, (int) std::lround (fs * 0.4));
        gateStep    = std::max (1, (int) std::lround (fs * 0.1));

        shortBuf.assign ((size_t) shortWindow, 0.0f);
        gateBuf.assign ((size_t) gateWindow, 0.0f);
        blockEnergy.assign ((size_t) kMaxBlocks, 0.0f);

        tpHist.assign ((size_t) numCh, std::array<float, 3> { 0.0f, 0.0f, 0.0f });
        tpPrimed.assign ((size_t) numCh, 0);

        reset();
    }

    void reset()
    {
        std::fill (shortBuf.begin(), shortBuf.end(), 0.0f);
        std::fill (gateBuf.begin(), gateBuf.end(), 0.0f);
        shortSum = 0.0;
        gateSum  = 0.0;
        shortPos = 0;
        gatePos  = 0;
        samplesSinceGateLog = 0;
        blockWrite = 0;
        blockCount = 0;
        cachedIntegratedLU = kFloorLU;
        truePeakLinear = 0.0f;

        for (auto& b : shelf) b.reset();
        for (auto& b : hpf)   b.reset();
        for (auto& h : tpHist) h.fill (0.0f);
        std::fill (tpPrimed.begin(), tpPrimed.end(), 0);
    }

    // chans: numChannelsThisCall de-interleaved pointers, at the plugin's
    // real output sample rate (not the internal 2x-oversampled engine rate).
    void process (const float* const* chans, int numChannelsThisCall, int n)
    {
        const int nc = std::min (numChannelsThisCall, numCh);

        for (int i = 0; i < n; ++i)
        {
            double weightedSumSq = 0.0;
            for (int c = 0; c < nc; ++c)
            {
                const float raw = chans[c][i];
                pushTruePeak (c, raw);

                float y = shelf[(size_t) c].process (raw);
                y = hpf[(size_t) c].process (y);
                weightedSumSq += (double) y * (double) y;
            }

            shortSum += weightedSumSq - (double) shortBuf[(size_t) shortPos];
            shortBuf[(size_t) shortPos] = (float) weightedSumSq;
            shortPos = (shortPos + 1) % shortWindow;

            gateSum += weightedSumSq - (double) gateBuf[(size_t) gatePos];
            gateBuf[(size_t) gatePos] = (float) weightedSumSq;
            gatePos = (gatePos + 1) % gateWindow;

            if (++samplesSinceGateLog >= gateStep)
            {
                samplesSinceGateLog = 0;
                logGateBlock (gateSum / (double) gateWindow);
            }
        }
    }

    float getShortTermLUFS() const
    {
        return (float) loudnessOf (shortSum / (double) shortWindow);
    }

    float getIntegratedLUFS() const { return (float) cachedIntegratedLU; }

    float getTruePeakDB() const
    {
        return 20.0f * std::log10 (std::max (truePeakLinear, 1.0e-9f));
    }

private:
    static constexpr double kLogFloor = 1.0e-15; // avoid log10(0)
    static constexpr double kFloorLU  = -70.0;   // ITU-R BS.1770 absolute gate
    static constexpr int    kMaxBlocks = 12000;  // 100 ms/block -> 20 minutes
    static constexpr int    kOversample = 4;

    static double loudnessOf (double meanSquare)
    {
        return -0.691 + 10.0 * std::log10 (std::max (meanSquare, kLogFloor));
    }

    void logGateBlock (double meanSq)
    {
        if (loudnessOf (meanSq) <= kFloorLU)
            return; // absolute gate

        blockEnergy[(size_t) blockWrite] = (float) meanSq;
        blockWrite = (blockWrite + 1) % kMaxBlocks;
        blockCount = std::min (blockCount + 1, kMaxBlocks);

        recomputeIntegrated();
    }

    void recomputeIntegrated()
    {
        if (blockCount == 0)
        {
            cachedIntegratedLU = kFloorLU;
            return;
        }

        double sumEnergy = 0.0;
        for (int i = 0; i < blockCount; ++i)
            sumEnergy += (double) blockEnergy[(size_t) i];
        const double ungatedMean   = sumEnergy / (double) blockCount;
        const double relativeGateLU = loudnessOf (ungatedMean) - 10.0;

        double gatedSum = 0.0;
        int gatedCount = 0;
        for (int i = 0; i < blockCount; ++i)
        {
            const double e = (double) blockEnergy[(size_t) i];
            if (loudnessOf (e) > relativeGateLU)
            {
                gatedSum += e;
                ++gatedCount;
            }
        }

        cachedIntegratedLU = gatedCount > 0 ? loudnessOf (gatedSum / (double) gatedCount)
                                            : loudnessOf (ungatedMean);
    }

    // 4x oversample via Catmull-Rom (can overshoot its bracketing samples,
    // unlike linear interpolation, which is why it can actually catch
    // inter-sample overs).
    void pushTruePeak (int c, float x)
    {
        auto& h = tpHist[(size_t) c];
        const float p0 = h[0], p1 = h[1], p2 = h[2];
        h[0] = p1; h[1] = p2; h[2] = x;

        if (tpPrimed[(size_t) c] < 3)
        {
            ++tpPrimed[(size_t) c];
            truePeakLinear = std::max (truePeakLinear, std::fabs (x));
            return;
        }

        const float a = p0, b = p1, cc = p2, d = x;
        truePeakLinear = std::max (truePeakLinear, std::fabs (cc));
        for (int k = 1; k < kOversample; ++k)
        {
            const float t  = (float) k / (float) kOversample;
            const float t2 = t * t, t3 = t2 * t;
            const float v = 0.5f * ( (2.0f * b)
                                   + (-a + cc) * t
                                   + (2.0f * a - 5.0f * b + 4.0f * cc - d) * t2
                                   + (-a + 3.0f * b - 3.0f * cc + d) * t3 );
            truePeakLinear = std::max (truePeakLinear, std::fabs (v));
        }
    }

    double fs = 48000.0;
    int numCh = 2;

    std::vector<Biquad> shelf, hpf;

    int shortWindow = 1, gateWindow = 1, gateStep = 1;
    std::vector<float> shortBuf, gateBuf;
    double shortSum = 0.0, gateSum = 0.0;
    int shortPos = 0, gatePos = 0, samplesSinceGateLog = 0;

    std::vector<float> blockEnergy;
    int blockWrite = 0, blockCount = 0;
    double cachedIntegratedLU = kFloorLU;

    std::vector<std::array<float, 3>> tpHist;
    std::vector<int> tpPrimed;
    float truePeakLinear = 0.0f;
};

} // namespace mc2
