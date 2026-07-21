#pragma once

#include "MC2LookAndFeel.h"

#include <juce_dsp/juce_dsp.h>

#include <array>

// ---------------------------------------------------------------------------
// Real-time FFT spectrum of the plugin's (post-processing, mono-summed)
// output, log-frequency-scaled like a hardware spectrum analyzer bolted
// onto the front panel. Not calibrated to any reference level - a glance
// view, not a measurement tool.
// ---------------------------------------------------------------------------
class SpectrumAnalyzer : public juce::Component
{
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize  = 1 << kFftOrder; // 2048, must match MC2AudioProcessor::kScopeSize

    SpectrumAnalyzer()
        : fft (kFftOrder), window (kFftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        setInterceptsMouseClicks (false, false);
        magnitudesDB.fill (-100.0f);
    }

    void setSampleRate (double sr) noexcept { sampleRate = sr > 0.0 ? sr : sampleRate; }

    // ring: exactly kFftSize samples; writePos = index of the next slot to
    // be written (one past the most recent sample). No locking on the
    // caller's side either - see the comment on MC2AudioProcessor::scopeBuffer.
    void update (const std::array<float, (size_t) kFftSize>& ring, int writePos)
    {
        for (int i = 0; i < kFftSize; ++i)
        {
            const int idx = (writePos + i) % kFftSize;
            fftData[(size_t) i] = ring[(size_t) idx];
        }

        window.multiplyWithWindowingTable (fftData.data(), (size_t) kFftSize);
        std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int i = 0; i < kFftSize / 2; ++i)
        {
            const float mag = fftData[(size_t) i] / (float) kFftSize;
            magnitudesDB[(size_t) i] = juce::Decibels::gainToDecibels (mag, -100.0f);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;

        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff05070a));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colour (0xff343d48));
        g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 1.2f);

        auto plot = r.reduced (10.0f, 14.0f);

        g.setColour (Colours::white.withAlpha (0.05f));
        for (float freq : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = plot.getX() + plot.getWidth() * xForFreq (freq);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        }

        constexpr float minDB = -90.0f, maxDB = 0.0f;
        const float binHz = (float) (sampleRate / (double) kFftSize);
        const float nyquist = (float) (sampleRate * 0.5);

        Path path;
        bool started = false;
        for (int i = 1; i < kFftSize / 2; ++i) // skip bin 0 (DC)
        {
            const float freq = (float) i * binHz;
            if (freq < 20.0f || freq > nyquist)
                continue;

            const float x = plot.getX() + plot.getWidth() * xForFreq (freq);
            const float db = jlimit (minDB, maxDB, magnitudesDB[(size_t) i]);
            const float y = plot.getY() + plot.getHeight() * (1.0f - (db - minDB) / (maxDB - minDB));

            if (! started) { path.startNewSubPath (x, y); started = true; }
            else            path.lineTo (x, y);
        }

        g.setColour (mc2gui::amber.withAlpha (0.85f));
        g.strokePath (path, PathStrokeType (1.2f));

        g.setColour (mc2gui::silkDim);
        g.setFont (mc2gui::silkFont (8.0f));
        g.drawText ("SPECTRUM", r.reduced (10.0f, 2.0f).withHeight (11.0f), Justification::centredLeft);
    }

private:
    static float xForFreq (float freq)
    {
        static const float lo = std::log10 (20.0f), hi = std::log10 (20000.0f);
        return juce::jlimit (0.0f, 1.0f,
                             (std::log10 (juce::jmax (20.0f, freq)) - lo) / (hi - lo));
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, (size_t) (kFftSize * 2)> fftData {};
    std::array<float, (size_t) (kFftSize / 2)> magnitudesDB {};
    double sampleRate = 44100.0;
};
