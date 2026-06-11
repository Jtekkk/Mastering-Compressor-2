#pragma once

#include "MC2LookAndFeel.h"

// ---------------------------------------------------------------------------
// Large illuminated Sifam-style moving-coil VU meter.
//
// Classic cream dial, true logarithmic VU arc geometry, red overload zone,
// warm lamp illumination, glass reflection and proper 300 ms needle
// ballistics. In GR mode the needle rests on 0 VU and kicks backwards with
// gain reduction, exactly like a vari-mu meter wired across the sidechain.
// ---------------------------------------------------------------------------

class VUMeter : public juce::Component
{
public:
    enum class Mode { Output, GainReduction };

    explicit VUMeter (juce::String sideLabel) : side (std::move (sideLabel))
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
    }

    // Called from the editor's UI timer (~30 Hz).
    void setTarget (float vu, Mode m)
    {
        if (m != mode)
        {
            mode = m;
            repaint();
        }
        targetVU = juce::jlimit (-30.0f, 5.0f, vu);
        // VU ballistics: ~300 ms to 99% => tau ~65 ms, evaluated per frame.
        needleVU += 0.40f * (targetVU - needleVU);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;

        auto r = getLocalBounds().toFloat();

        // housing + bezel
        g.setColour (Colour (0xff0a0d11));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (Colour (0xff343d48));
        g.drawRoundedRectangle (r.reduced (2.5f), 7.0f, 1.6f);
        drawScrews (g, r);

        auto face = r.reduced (16.0f);

        // cream face with warm lamp illumination from the top
        {
            ColourGradient base (Colour (0xfff6edd6), face.getCentreX(), face.getY(),
                                 Colour (0xffe2d5b4), face.getCentreX(), face.getBottom(), false);
            g.setGradientFill (base);
            g.fillRoundedRectangle (face, 4.0f);

            ColourGradient lamp (mc2gui::amber.withAlpha (0.30f),
                                 face.getCentreX(), face.getY() + face.getHeight() * 0.10f,
                                 mc2gui::amber.withAlpha (0.0f),
                                 face.getCentreX(), face.getBottom(), true);
            lamp.isRadial = true;
            g.setGradientFill (lamp);
            g.fillRoundedRectangle (face, 4.0f);
        }

        const float cx = face.getCentreX();
        const float pivotY = face.getBottom() + face.getHeight() * 0.62f;
        const float rTip = pivotY - face.getY() - 16.0f;

        auto pointAt = [&] (float deg, float rad) {
            const float a = degreesToRadians (deg);
            return Point<float> (cx + std::sin (a) * rad, pivotY - std::cos (a) * rad);
        };

        // red overload arc 0..+3
        {
            Path arc;
            arc.addCentredArc (cx, pivotY, rTip + 1.0f, rTip + 1.0f, 0.0f,
                               degreesToRadians (angleFor (0.0f)),
                               degreesToRadians (angleFor (3.0f)), true);
            g.setColour (Colour (0xffa32c22));
            g.strokePath (arc, PathStrokeType (3.2f));
        }

        // scale ticks + numerals
        struct Mark { float vu; const char* txt; bool major; };
        static constexpr Mark marks[] = {
            { -20.0f, "20", true }, { -15.0f, nullptr, false }, { -10.0f, "10", true },
            { -8.0f, nullptr, false }, { -7.0f, "7", true }, { -6.0f, nullptr, false },
            { -5.0f, "5", true }, { -4.0f, nullptr, false }, { -3.0f, "3", true },
            { -2.0f, "2", true }, { -1.0f, "1", true }, { -0.5f, nullptr, false },
            { 0.0f, "0", true }, { 0.5f, nullptr, false }, { 1.0f, "1", true },
            { 2.0f, "2", true }, { 3.0f, "3", true }
        };

        for (const auto& m : marks)
        {
            const float a = angleFor (m.vu);
            const bool red = m.vu >= 0.0f && ! (m.vu == 0.0f);
            g.setColour (red ? Colour (0xffa32c22) : Colour (0xff272018));
            const float len = m.major ? 9.0f : 5.0f;
            g.drawLine ({ pointAt (a, rTip - len), pointAt (a, rTip) }, m.major ? 1.5f : 1.0f);

            if (m.txt != nullptr)
            {
                g.setFont (mc2gui::silkFont (m.vu == 0.0f ? 11.0f : 9.0f));
                const auto p = pointAt (a, rTip - 17.0f);
                g.drawText (m.txt, Rectangle<int> ((int) p.x - 10, (int) p.y - 6, 20, 12),
                            Justification::centred);
            }
        }

        // legends
        g.setColour (Colour (0xff272018));
        g.setFont (mc2gui::silkFont (16.0f));
        g.drawText (mode == Mode::Output ? "VU" : "GR",
                    Rectangle<int> ((int) cx - 20, (int) (face.getBottom() - 46), 40, 18),
                    Justification::centred);
        g.setFont (mc2gui::silkFont (7.5f));
        g.drawText (String::fromUTF8 (mode == Mode::Output
                                          ? "OUTPUT LEVEL  \xc2\xb7  0 VU = -18 dBFS"
                                          : "GAIN REDUCTION  \xc2\xb7  dB BELOW 0"),
                    Rectangle<int> ((int) cx - 90, (int) (face.getBottom() - 28), 180, 10),
                    Justification::centred);
        g.setColour (Colour (0xff272018).withAlpha (0.65f));
        g.setFont (mc2gui::silkFont (8.0f));
        g.drawText (side, Rectangle<int> ((int) face.getX() + 6, (int) (face.getBottom() - 16), 60, 10),
                    Justification::centredLeft);
        g.drawText ("MC-2", Rectangle<int> ((int) face.getRight() - 66, (int) (face.getBottom() - 16), 60, 10),
                    Justification::centredRight);

        // needle (clipped to the dial)
        {
            Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (face.toNearestInt());

            const float a = angleFor (needleVU);
            g.setColour (Colour (0x33000000));
            g.drawLine ({ pointAt (a, 6.0f).translated (2.0f, 2.0f),
                          pointAt (a, rTip - 3.0f).translated (2.0f, 2.0f) }, 2.4f);
            g.setColour (Colour (0xff43281e));
            g.drawLine ({ pointAt (a, 6.0f), pointAt (a, rTip - 3.0f) }, 2.2f);
        }

        // glass reflection
        {
            ColourGradient gl (Colours::white.withAlpha (0.10f), face.getX(), face.getY(),
                               Colours::white.withAlpha (0.0f), face.getX() + face.getWidth() * 0.55f,
                               face.getBottom(), false);
            g.setGradientFill (gl);
            g.fillRoundedRectangle (face, 4.0f);
            g.setColour (Colour (0xff0a0d11).withAlpha (0.55f));
            g.drawRoundedRectangle (face, 4.0f, 1.5f);
        }
    }

private:
    // True VU dial geometry: needle deflection follows 10^(vu/20).
    static float angleFor (float vu)
    {
        const float v = juce::jlimit (-23.0f, 3.8f, vu);
        float p = std::pow (10.0f, v / 20.0f) / 1.4125f;   // 1.0 at +3 VU
        p = juce::jlimit (0.045f, 1.045f, p);
        return -43.0f + 86.0f * (p - 0.045f) / (1.0f - 0.045f);
    }

    static void drawScrews (juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (auto corner : { r.getTopLeft(), r.getTopRight(), r.getBottomLeft(), r.getBottomRight() })
        {
            const auto c = corner.translated (corner.x < r.getCentreX() ? 8.0f : -8.0f,
                                              corner.y < r.getCentreY() ? 8.0f : -8.0f);
            g.setColour (juce::Colour (0xff4a525c));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c));
            g.setColour (juce::Colour (0xff181d23));
            g.drawLine (c.x - 2.2f, c.y - 1.2f, c.x + 2.2f, c.y + 1.2f, 1.1f);
        }
    }

    juce::String side;
    Mode mode = Mode::GainReduction;
    float needleVU = -23.0f, targetVU = -23.0f;
};

// ---------------------------------------------------------------------------
// Twin 5670 tube window: two glowing bottles behind smoked glass. The glow
// breathes with gain reduction — the harder the unit works, the warmer it
// looks.
// ---------------------------------------------------------------------------
class TubeWindow : public juce::Component
{
public:
    TubeWindow() { setInterceptsMouseClicks (false, false); }

    void setGlow (float g01)
    {
        glow += 0.25f * (juce::jlimit (0.0f, 1.0f, g01) - glow);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;

        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff05070a));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (Colour (0xff343d48));
        g.drawRoundedRectangle (r.reduced (1.5f), 6.0f, 1.4f);

        for (float fx : { 0.32f, 0.68f })
            drawTube (g, { r.getX() + r.getWidth() * fx, r.getCentreY() + 4.0f },
                      r.getHeight() * 0.36f);

        g.setColour (mc2gui::silkDim);
        g.setFont (mc2gui::silkFont (8.0f));
        g.drawText ("5670 x2", r.reduced (8.0f).removeFromTop (12.0f),
                    Justification::centredTop);
    }

private:
    void drawTube (juce::Graphics& g, juce::Point<float> c, float halfH)
    {
        using namespace juce;

        const float w = halfH * 0.62f;
        auto glass = Rectangle<float> (c.x - w * 0.5f, c.y - halfH, w, halfH * 2.0f);

        // envelope
        g.setColour (Colour (0xff12161c));
        g.fillRoundedRectangle (glass, w * 0.5f);

        // heater glow
        const float a = 0.28f + 0.55f * glow;
        ColourGradient gl (mc2gui::amber.withAlpha (a), c.x, c.y + halfH * 0.25f,
                           mc2gui::amber.withAlpha (0.0f), c.x, c.y - halfH * 0.9f, true);
        gl.isRadial = true;
        g.setGradientFill (gl);
        g.fillRoundedRectangle (glass.expanded (6.0f + 4.0f * glow), w);

        // plate structure
        g.setColour (Colour (0xff1d232b));
        g.fillRect (Rectangle<float> (w * 0.42f, halfH * 1.1f).withCentre (c));
        g.setColour (mc2gui::amber.withAlpha (jmin (1.0f, a + 0.25f)));
        g.fillEllipse (Rectangle<float> (3.5f, 7.0f).withCentre ({ c.x, c.y + halfH * 0.3f }));

        // glass outline + getter flash
        g.setColour (Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (glass, w * 0.5f, 1.1f);
        g.setColour (Colours::silver.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (w * 0.45f, 5.0f)
                           .withCentre ({ c.x, c.y - halfH + 5.0f }));
    }

    float glow = 0.0f;
};
