#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ---------------------------------------------------------------------------
// Faceplate styling: steel-blue panel, cream silkscreen, black detented
// knobs with cream pointers, chicken-head rotary switches and paddle
// toggles. Everything is drawn as vectors — no image assets.
// ---------------------------------------------------------------------------

namespace mc2gui
{
    const juce::Colour panelTop    { 0xff242f3d };
    const juce::Colour panelBottom { 0xff141b24 };
    const juce::Colour silk        { 0xffe9dec5 };
    const juce::Colour silkDim     { 0xff958c75 };
    const juce::Colour accentRed   { 0xffa3372b };
    const juce::Colour knobFace    { 0xff1d242d };
    const juce::Colour knobDark    { 0xff0c0f13 };
    const juce::Colour pointerCol  { 0xfff1e7cd };
    const juce::Colour amber       { 0xffffb35c };

    inline juce::Font silkFont (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain))
                   .withExtraKerningFactor (0.06f);
    }
}

class MC2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    MC2LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, mc2gui::silk);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, mc2gui::amber.withAlpha (0.4f));
        setColour (juce::Label::textColourId, mc2gui::silk);
        setColour (juce::TextEditor::textColourId, mc2gui::silk);
        setColour (juce::TextEditor::highlightColourId, mc2gui::amber.withAlpha (0.4f));
        setColour (juce::TextEditor::focusedOutlineColourId, mc2gui::silkDim);

        // preset browser / undo-redo strip
        setColour (juce::ComboBox::textColourId, mc2gui::silk);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff20252c));
        setColour (juce::ComboBox::outlineColourId, mc2gui::silkDim.withAlpha (0.5f));
        setColour (juce::ComboBox::arrowColourId, mc2gui::silkDim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c2026));
        setColour (juce::PopupMenu::textColourId, mc2gui::silk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, mc2gui::amber.withAlpha (0.25f));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff20252c));
        setColour (juce::TextButton::textColourOffId, mc2gui::silkDim);
        setColour (juce::TextButton::textColourOnId, mc2gui::silk);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return mc2gui::silkFont (12.0f, false);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        using namespace juce;

        const auto& props = slider.getProperties();
        const bool isSwitch = props.contains ("switchLabels");

        StringArray labels;
        if (isSwitch)
            labels = StringArray::fromTokens (props["switchLabels"].toString(), "|", "");

        const auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY() + (isSwitch ? 8.0f : 0.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f
                             - (isSwitch ? 28.0f : 13.0f);
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        auto pointAt = [&] (float a, float r) {
            return Point<float> (cx + std::sin (a) * r, cy - std::cos (a) * r);
        };

        // --- detent ticks -------------------------------------------------
        const int nTicks = isSwitch ? labels.size()
                                    : (int) props.getWithDefault ("tickCount", 11);
        const int selected = isSwitch
            ? roundToInt (sliderPos * (float) (jmax (1, nTicks - 1)))
            : -1;

        for (int i = 0; i < nTicks; ++i)
        {
            const float f = nTicks > 1 ? (float) i / (float) (nTicks - 1) : 0.0f;
            const float a = rotaryStartAngle + f * (rotaryEndAngle - rotaryStartAngle);
            const bool hot = isSwitch && i == selected;
            g.setColour (hot ? mc2gui::amber : mc2gui::silkDim.withAlpha (0.75f));
            const float r1 = radius + 3.0f, r2 = radius + (hot ? 8.0f : 6.5f);
            g.drawLine ({ pointAt (a, r1), pointAt (a, r2) }, hot ? 1.8f : 1.1f);
        }

        // --- switch position labels ----------------------------------------
        if (isSwitch)
        {
            g.setFont (mc2gui::silkFont (8.2f));
            for (int i = 0; i < labels.size(); ++i)
            {
                const float f = labels.size() > 1 ? (float) i / (float) (labels.size() - 1) : 0.0f;
                const float a = rotaryStartAngle + f * (rotaryEndAngle - rotaryStartAngle);

                // stagger alternate labels outwards so neighbours never collide
                const float rl = radius + 14.0f + (i % 2 == 1 ? 11.0f : 0.0f);
                const auto p = pointAt (a, rl);

                auto box = Rectangle<int> ((int) p.x - 20, (int) p.y - 5, 40, 11);
                box.setX (juce::jlimit (x, juce::jmax (x, x + width - box.getWidth()), box.getX()));

                g.setColour (i == selected ? mc2gui::silk : mc2gui::silkDim);
                g.drawFittedText (labels[i], box, Justification::centred, 1);
            }
        }

        // --- knob body ------------------------------------------------------
        {
            const auto body = Rectangle<float> (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
            ColourGradient grad (mc2gui::knobFace.brighter (0.25f), cx - radius * 0.5f, cy - radius * 0.6f,
                                 mc2gui::knobDark, cx + radius * 0.6f, cy + radius * 0.8f, true);
            g.setGradientFill (grad);
            g.fillEllipse (body);
            g.setColour (Colour (0xff3a434f));
            g.drawEllipse (body, 1.4f);
            g.setColour (mc2gui::knobDark);
            g.fillEllipse (body.reduced (radius * 0.24f));
        }

        // --- pointer ----------------------------------------------------------
        if (isSwitch)
        {
            // chicken-head wedge
            Path p;
            p.startNewSubPath (-radius * 0.34f, radius * 0.42f);
            p.lineTo (0.0f, -radius * 0.97f);
            p.lineTo (radius * 0.34f, radius * 0.42f);
            p.closeSubPath();
            p = p.createPathWithRoundedCorners (4.0f);

            g.setColour (Colour (0xff10141a));
            g.fillPath (p, AffineTransform::rotation (angle).translated (cx, cy));

            Path tip;
            tip.addRoundedRectangle (-1.6f, -radius * 0.92f, 3.2f, radius * 0.55f, 1.6f);
            g.setColour (mc2gui::pointerCol);
            g.fillPath (tip, AffineTransform::rotation (angle).translated (cx, cy));
        }
        else
        {
            Path tip;
            tip.addRoundedRectangle (-2.0f, -radius * 0.95f, 4.0f, radius * 0.62f, 2.0f);
            g.setColour (mc2gui::pointerCol);
            g.fillPath (tip, AffineTransform::rotation (angle).translated (cx, cy));
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool) override
    {
        using namespace juce;

        const auto& props = button.getProperties();
        const String onLbl  = props.getWithDefault ("onLabel", "ON").toString();
        const String offLbl = props.getWithDefault ("offLabel", "OFF").toString();
        const bool on = button.getToggleState();

        auto b = button.getLocalBounds().toFloat();
        auto top = b.removeFromTop (13.0f);
        auto bot = b.removeFromBottom (13.0f);

        g.setFont (mc2gui::silkFont (9.0f));
        g.setColour (on ? mc2gui::silk : mc2gui::silkDim);
        g.drawText (onLbl, top, Justification::centred);
        g.setColour (on ? mc2gui::silkDim : mc2gui::silk);
        g.drawText (offLbl, bot, Justification::centred);

        // slot
        const float slotW = jmin (24.0f, b.getWidth() * 0.5f);
        auto slot = Rectangle<float> (slotW, b.getHeight() - 8.0f)
                        .withCentre (b.getCentre());
        g.setColour (Colour (0xff070a0d));
        g.fillRoundedRectangle (slot, slotW * 0.5f);
        g.setColour (Colour (0xff3a434f));
        g.drawRoundedRectangle (slot, slotW * 0.5f, 1.2f);

        // paddle lever
        const float leverR = slotW * 0.62f;
        const auto centre = slot.getCentre();
        const auto knobPos = Point<float> (centre.x, on ? slot.getY() + leverR * 0.8f
                                                        : slot.getBottom() - leverR * 0.8f);
        // stem
        Path stem;
        stem.addQuadrilateral (centre.x - 3.5f, centre.y,
                               centre.x + 3.5f, centre.y,
                               knobPos.x + 5.0f, knobPos.y,
                               knobPos.x - 5.0f, knobPos.y);
        g.setColour (Colour (0xff8d9298));
        g.fillPath (stem);

        ColourGradient grad (Colour (0xffd9dadc), knobPos.x - leverR * 0.4f, knobPos.y - leverR * 0.4f,
                             Colour (0xff70757c), knobPos.x + leverR * 0.5f, knobPos.y + leverR * 0.5f, true);
        g.setGradientFill (grad);
        g.fillEllipse (Rectangle<float> (leverR * 2.0f, leverR * 2.0f).withCentre (knobPos));
        g.setColour (Colour (0xff14181d));
        g.drawEllipse (Rectangle<float> (leverR * 2.0f, leverR * 2.0f).withCentre (knobPos), 1.2f);

        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour (mc2gui::amber.withAlpha (0.12f));
            g.fillRoundedRectangle (button.getLocalBounds().toFloat(), 6.0f);
        }
    }
};
