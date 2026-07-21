#pragma once

#include "MC2LookAndFeel.h"

#include <vector>

// ---------------------------------------------------------------------------
// Scrolling gain-reduction history: a dark strip with an amber trace of the
// last ~10 s of GR, oldest at the left, newest at the right. 0 dB sits at
// the top; deeper reduction pulls the trace down, same sense as the VU
// meter's GR needle kicking backwards.
// ---------------------------------------------------------------------------
class GrHistoryGraph : public juce::Component
{
public:
    GrHistoryGraph() : history ((size_t) kCapacity, 0.0f)
    {
        setInterceptsMouseClicks (false, false);
    }

    // grDB: positive dB of gain reduction. Call once per UI-timer tick.
    void pushSample (float grDB)
    {
        history[(size_t) writePos] = juce::jlimit (0.0f, kMaxDB, grDB);
        writePos = (writePos + 1) % kCapacity;
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
        for (float d = 6.0f; d < kMaxDB; d += 6.0f)
        {
            const float y = plot.getY() + plot.getHeight() * (d / kMaxDB);
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }

        Path fillPath, linePath;
        const int n = kCapacity;
        for (int i = 0; i < n; ++i)
        {
            const int idx = (writePos + i) % kCapacity;
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (n - 1);
            const float gr = history[(size_t) idx];
            const float y = plot.getY() + plot.getHeight() * jlimit (0.0f, 1.0f, gr / kMaxDB);

            if (i == 0)
            {
                linePath.startNewSubPath (x, y);
                fillPath.startNewSubPath (x, plot.getBottom());
                fillPath.lineTo (x, y);
            }
            else
            {
                linePath.lineTo (x, y);
                fillPath.lineTo (x, y);
            }
        }
        fillPath.lineTo (plot.getRight(), plot.getBottom());
        fillPath.closeSubPath();

        g.setColour (mc2gui::amber.withAlpha (0.18f));
        g.fillPath (fillPath);
        g.setColour (mc2gui::amber.withAlpha (0.85f));
        g.strokePath (linePath, PathStrokeType (1.4f));

        g.setColour (mc2gui::silkDim);
        g.setFont (mc2gui::silkFont (8.0f));
        g.drawText ("GR HISTORY", r.reduced (10.0f, 2.0f).withHeight (11.0f),
                    Justification::centredLeft);
        g.drawText (String ((int) kMaxDB) + " dB", r.reduced (10.0f, 2.0f).withHeight (11.0f),
                    Justification::centredRight);
    }

private:
    static constexpr int kCapacity = 300;  // ~10 s @ 30 Hz
    static constexpr float kMaxDB = 24.0f;

    std::vector<float> history;
    int writePos = 0;
};
