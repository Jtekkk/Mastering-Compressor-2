#pragma once

#include "Biquad.h"
#include "PassiveEQ.h"
#include "Rectifiers.h"
#include "SidechainEQ.h"
#include "TubeStage.h"

#include <algorithm>
#include <cmath>

namespace mc2
{

inline float dbToLin (float dB) noexcept  { return std::pow (10.0f, dB * 0.05f); }
inline float linToDb (float v)  noexcept  { return 20.0f * std::log10 (v + 1.0e-9f); }

// Quadratic soft knee: returns the "effective overshoot" in dB.
inline float softKnee (float overDB, float kneeDB) noexcept
{
    const float half = kneeDB * 0.5f;
    if (overDB <= -half) return 0.0f;
    if (overDB >=  half) return overDB;
    const float t = overDB + half;
    return t * t / (2.0f * kneeDB);
}

// ---------------------------------------------------------------------------
// Control state for the whole unit. All values arrive already stepped /
// detented from the parameter layer.
// ---------------------------------------------------------------------------
struct EngineParams
{
    float inputGainDB  = 0.0f;    // balanced input stage trim
    float thresholdDB  = -12.0f;
    float attackMs     = 50.0f;   // 25..70 ms
    int   recoveryIdx  = 1;       // 0.2 / 0.4 / 0.6 / 4 / 8 s
    int   rectifierIdx = 0;       // six rectifier circuits
    int   scEqIdx      = 0;       // four sidechain curves
    bool  limitMode    = false;   // false: COMPRESS 1.5:1, true: LIMIT 4:1..20:1
    bool  stereoLink   = true;
    bool  eqIn         = false;
    float eqLowDB      = 0.0f;    // passive EQ, 0..+6
    float eqAirDB      = 0.0f;
    float outGainDB    = 0.0f;    // balanced output stage trim
};

// ---------------------------------------------------------------------------
// The full twin-channel engine. Feedback topology: the sidechain listens to
// the *output* of the variable-mu stage, exactly like the hardware. With a
// proportional control law GR = k * overshoot(output), the input-referred
// ratio is R = 1 + k, so:
//
//     COMPRESS  k = 0.5                        ->  1.5:1, knee 6 dB
//     LIMIT     k = 3 .. 19 (rises with drive) ->  4:1 climbing to 20:1
//
// The ratio that stiffens as you push deeper into it is the signature
// vari-mu limiting behaviour.
// ---------------------------------------------------------------------------
class MC2Engine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        fs = sampleRate;

        for (auto& c : ch)
        {
            c.scEq.prepare (fs);
            c.passiveEq.prepare (fs);
            c.outIron.highpass (fs, 5.0, 0.707);
            c.outIron.reset();
            c.reset();
        }

        smoothCoef = onePole (0.015f);          // 15 ms trim-gain smoothing
        cvCoef     = onePole (0.002f);          // tube grid RC, anti-zipper
        optoCoef   = onePole (0.040f);          // photo-cell memory

        forceUpdate = true;
        setParams (params);
        for (auto& c : ch)
        {
            c.inGain  = dbToLin (params.inputGainDB);
            c.outGain = dbToLin (params.outGainDB);
        }
    }

    void setParams (const EngineParams& p)
    {
        const bool ballisticsChanged = forceUpdate
            || p.attackMs     != params.attackMs
            || p.recoveryIdx  != params.recoveryIdx
            || p.rectifierIdx != params.rectifierIdx;

        const bool eqChanged = forceUpdate
            || p.eqLowDB != params.eqLowDB
            || p.eqAirDB != params.eqAirDB;

        const bool scChanged = forceUpdate || p.scEqIdx != params.scEqIdx;

        params = p;
        forceUpdate = false;

        if (ballisticsChanged)
        {
            const auto traits = rectifierTraits (static_cast<Rectifier> (params.rectifierIdx));
            static constexpr float recoverySec[5] = { 0.2f, 0.4f, 0.6f, 4.0f, 8.0f };
            const int rIdx = std::clamp (params.recoveryIdx, 0, 4);

            attackCoef  = onePole (0.001f * params.attackMs * traits.attackScale);
            releaseCoef = onePole (recoverySec[rIdx] * traits.releaseScale);
        }

        if (scChanged)
            for (auto& c : ch)
                c.scEq.setCurve (params.scEqIdx);

        if (eqChanged)
            for (auto& c : ch)
                c.passiveEq.setGains (params.eqLowDB, params.eqAirDB);
    }

    // Planar in-place processing; numCh is 1 or 2.
    void process (float* const* audio, int numCh, int n)
    {
        numCh = std::clamp (numCh, 1, maxChannels);

        const auto rect   = static_cast<Rectifier> (params.rectifierIdx);
        const auto traits = rectifierTraits (rect);
        const float inTarget  = dbToLin (params.inputGainDB);
        const float outTarget = dbToLin (params.outGainDB);
        const bool  link      = params.stereoLink && numCh == 2;
        const float kneeDB    = params.limitMode ? 3.0f : 6.0f;

        double sumSq[maxChannels] = { 0.0, 0.0 };

        for (int i = 0; i < n; ++i)
        {
            // ---- sidechain: rectify the fed-back mu-stage output ----------
            float det[maxChannels];
            for (int c = 0; c < numCh; ++c)
            {
                const float s = ch[c].scEq.process (ch[c].fb);
                det[c] = rectify (rect, s);
            }

            if (link)
            {
                const float shared = 0.5f * (det[0] + det[1]);
                det[0] = det[1] = shared;
            }

            for (int c = 0; c < numCh; ++c)
            {
                Channel& cc = ch[c];

                // trim-gain smoothing (click-free detented knobs)
                cc.inGain  += smoothCoef * (inTarget  - cc.inGain);
                cc.outGain += smoothCoef * (outTarget - cc.outGain);

                float x = audio[c][i] * cc.inGain;

                // ---- detector ballistics ------------------------------
                const float target = det[c];
                if (target > cc.env) cc.env += attackCoef  * (target - cc.env);
                else                 cc.env += releaseCoef * (target - cc.env);

                float lvl = traits.squaredLaw ? std::sqrt (std::max (cc.env, 0.0f))
                                              : cc.env;
                if (traits.optoStage)
                {
                    cc.opto += optoCoef * (lvl - cc.opto);
                    lvl = cc.opto;
                }

                // +3 dB detector calibration: average-reading rectifier vs
                // sine peak, so the THRESHOLD legend lines up with practice.
                const float lvlDB = linToDb (lvl) + 3.0f;
                const float over  = softKnee (lvlDB - params.thresholdDB, kneeDB);

                // feedback control law -> input-referred ratio 1 + k
                const float k = params.limitMode
                                  ? 3.0f + 16.0f * std::min (1.0f, over / 12.0f)
                                  : 0.5f;
                const float grTarget = std::min (k * over, 26.0f);

                // tube grid RC
                cc.gr += cvCoef * (grTarget - cc.gr);

                // ---- twin-tube gain element ----------------------------
                const float mu = dbToLin (-cc.gr);
                float y = cc.tubes.process (x, cc.gr, mu);
                cc.fb = y;                          // feedback tap

                // ---- passive EQ, output stage --------------------------
                if (params.eqIn)
                    y = cc.passiveEq.process (y);

                y *= cc.outGain;
                y = static_cast<float> (cc.outIron.process (y)); // output iron LF
                y = railLimit (y);                                // 120 V headroom

                audio[c][i] = y;
                sumSq[c] += static_cast<double> (y) * y;
            }
        }

        for (int c = 0; c < numCh; ++c)
        {
            meterGrDB[c] = ch[c].gr;
            meterRms[c]  = n > 0 ? static_cast<float> (std::sqrt (sumSq[c] / n)) : 0.0f;
        }
    }

    void reset()
    {
        for (auto& c : ch)
            c.reset();
    }

    float getGainReductionDB (int c) const noexcept { return meterGrDB[std::clamp (c, 0, 1)]; }
    float getOutputRms (int c) const noexcept       { return meterRms[std::clamp (c, 0, 1)]; }

private:
    struct Channel
    {
        SidechainEQ scEq;
        PassiveEQ   passiveEq;
        TubeStage   tubes;
        Biquad      outIron;

        float fb = 0.0f, env = 0.0f, opto = 0.0f, gr = 0.0f;
        float inGain = 1.0f, outGain = 1.0f;

        void reset()
        {
            fb = env = opto = gr = 0.0f;
            scEq.reset();
            outIron.reset();
        }
    };

    inline float onePole (float seconds) const noexcept
    {
        return 1.0f - std::exp (-1.0f / (static_cast<float> (fs) * std::max (1.0e-4f, seconds)));
    }

    double fs = 96000.0;
    EngineParams params;
    bool forceUpdate = true;

    float attackCoef = 0.01f, releaseCoef = 0.001f;
    float smoothCoef = 0.01f, cvCoef = 0.05f, optoCoef = 0.001f;

    Channel ch[maxChannels];
    float meterGrDB[maxChannels] = { 0.0f, 0.0f };
    float meterRms[maxChannels]  = { 0.0f, 0.0f };
};

} // namespace mc2
