#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    constexpr int kWidth  = 1180;
    constexpr int kTopBarH = 32;   // preset browser + undo/redo strip
    constexpr int kPanelH  = 580;  // the original front-panel artwork height
    constexpr int kHeight  = kPanelH + kTopBarH;
    constexpr int kMainRowY    = 312 + kTopBarH;
    constexpr int kMainRowH    = 122;
    constexpr int kLowerRowY   = 462 + kTopBarH;
    constexpr int kLowerRowH   = 100;

    constexpr float kPi = juce::MathConstants<float>::pi;

    const char* mainTitles[8]  = { "INPUT", "THRESHOLD", "ATTACK", "RECOVERY",
                                   "MODE", "RECTIFIER", "SC EQ", "OUTPUT" };
    const char* lowerTitles[7] = { "ST LINK", "PASSIVE EQ", "LOW 90 Hz", "AIR 12 kHz",
                                   "METER", "CAL L", "CAL R" };
}

MC2AudioProcessorEditor::MC2AudioProcessorEditor (MC2AudioProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (meterL);
    addAndMakeVisible (meterR);
    addAndMakeVisible (tubeWindow);

    setupKnob (inputKnob,     ParamID::input,     13);
    setupKnob (thresholdKnob, ParamID::threshold, 17);
    setupKnob (attackKnob,    ParamID::attack,    10);
    setupKnob (outputKnob,    ParamID::output,    13);
    setupKnob (eqLowKnob,     ParamID::eqLow,     13);
    setupKnob (eqAirKnob,     ParamID::eqAir,     13);
    setupKnob (calLKnob,      ParamID::calL,      13);
    setupKnob (calRKnob,      ParamID::calR,      13);

    // compact ring legends; the host sees the full choice names from Params.h
    setupSwitch (recoverySwitch,  ParamID::recovery,  { "0.2", "0.4", "0.6", "4", "8" });
    setupSwitch (rectifierSwitch, ParamID::rectifier, { "FW", "HW", "GE", "SI", "OPTO", "RMS" });
    setupSwitch (scEqSwitch,      ParamID::scEq,      { "FLAT", "HP100", "HP200", "HF5K" });

    setupToggle (modeToggle,   ParamID::mode,      "LIMIT",  "COMPRESS");
    setupToggle (linkToggle,   ParamID::link,      "LINK",   "DUAL");
    setupToggle (eqInToggle,   ParamID::eqIn,      "EQ IN",  "EQ OUT");
    setupToggle (meterToggle,  ParamID::meterMode, "OUTPUT", "GR");
    setupToggle (bypassToggle, ParamID::bypass,    "BYPASS", "OPERATE");

    pMeterMode = proc.apvts.getRawParameterValue (ParamID::meterMode);
    pCalL      = proc.apvts.getRawParameterValue (ParamID::calL);
    pCalR      = proc.apvts.getRawParameterValue (ParamID::calR);
    pBypass    = proc.apvts.getRawParameterValue (ParamID::bypass);

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.addItem ("Default", 1);
    {
        int itemId = 2;
        for (auto& preset : mc2presets::factoryPresets())
            presetBox.addItem (preset.name, itemId++);
    }
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0)
            proc.setCurrentProgram (id - 1);
    };

    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };

    startTimerHz (30);
    setSize (kWidth, kHeight);
}

MC2AudioProcessorEditor::~MC2AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MC2AudioProcessorEditor::setupKnob (juce::Slider& s, const char* paramID, int tickCount)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
    s.setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
    s.getProperties().set ("tickCount", tickCount);
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, paramID, s));
}

void MC2AudioProcessorEditor::setupSwitch (juce::Slider& s, const char* paramID,
                                           const juce::StringArray& labels)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    // centred at 12 o'clock, 30 degrees per position
    const float sweep = juce::degreesToRadians (30.0f) * (float) (labels.size() - 1);
    s.setRotaryParameters (2.0f * kPi - sweep * 0.5f, 2.0f * kPi + sweep * 0.5f, true);
    s.getProperties().set ("switchLabels", labels.joinIntoString ("|"));
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, paramID, s));
}

void MC2AudioProcessorEditor::setupToggle (juce::ToggleButton& b, const char* paramID,
                                           const char* onLabel, const char* offLabel)
{
    b.setButtonText ("");
    b.getProperties().set ("onLabel", onLabel);
    b.getProperties().set ("offLabel", offLabel);
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, paramID, b));
}

juce::Rectangle<int> MC2AudioProcessorEditor::mainStation (int index) const
{
    const float w = (float) kWidth / 8.0f;
    return juce::Rectangle<int> ((int) (w * (float) index), kMainRowY, (int) w, kMainRowH)
        .reduced (4, 0);
}

juce::Rectangle<int> MC2AudioProcessorEditor::lowerStation (int index) const
{
    const float w = (float) kWidth / 8.0f;
    return juce::Rectangle<int> ((int) (w * (float) index), kLowerRowY, (int) w, kLowerRowH)
        .reduced (4, 0);
}

void MC2AudioProcessorEditor::resized()
{
    // preset browser + undo/redo strip, fixed to the top of the window
    presetBox.setBounds (16, (kTopBarH - 22) / 2, 220, 22);
    redoButton.setBounds (kWidth - 16 - 64, (kTopBarH - 22) / 2, 64, 22);
    undoButton.setBounds (redoButton.getX() - 6 - 64, (kTopBarH - 22) / 2, 64, 22);

    meterL.setBounds (24, 46 + kTopBarH, 350, 244);
    meterR.setBounds (kWidth - 24 - 350, 46 + kTopBarH, 350, 244);
    tubeWindow.setBounds (462, 56 + kTopBarH, 256, 122);

    // bypass paddle under the tube window
    bypassToggle.setBounds (kWidth / 2 - 36, 208 + kTopBarH, 72, 82);

    inputKnob.setBounds     (mainStation (0));
    thresholdKnob.setBounds (mainStation (1));
    attackKnob.setBounds    (mainStation (2));
    recoverySwitch.setBounds (mainStation (3));
    modeToggle.setBounds    (mainStation (4).withSizeKeepingCentre (74, 96));
    rectifierSwitch.setBounds (mainStation (5));
    scEqSwitch.setBounds    (mainStation (6));
    outputKnob.setBounds    (mainStation (7));

    linkToggle.setBounds (lowerStation (0).withSizeKeepingCentre (74, 90));
    eqInToggle.setBounds (lowerStation (1).withSizeKeepingCentre (74, 90));
    eqLowKnob.setBounds  (lowerStation (2));
    eqAirKnob.setBounds  (lowerStation (3));
    meterToggle.setBounds (lowerStation (4).withSizeKeepingCentre (74, 90));
    calLKnob.setBounds   (lowerStation (5));
    calRKnob.setBounds   (lowerStation (6));
}

void MC2AudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;

    // preset / undo bar, fixed to the very top of the window
    g.setColour (Colour (0xff14171c));
    g.fillRect (0, 0, kWidth, kTopBarH);
    g.setColour (mc2gui::silkDim.withAlpha (0.4f));
    g.drawHorizontalLine (kTopBarH - 1, 0.0f, (float) kWidth);

    // panel
    g.setGradientFill (ColourGradient (mc2gui::panelTop, 0.0f, (float) kTopBarH,
                                       mc2gui::panelBottom, 0.0f, (float) kHeight, false));
    g.fillRect (0, kTopBarH, kWidth, kPanelH);

    // subtle brushed texture
    g.setColour (Colours::white.withAlpha (0.018f));
    for (int yy = kTopBarH + 8; yy < kHeight; yy += 7)
        g.drawHorizontalLine (yy, 0.0f, (float) kWidth);

    // header
    g.setColour (mc2gui::silk);
    g.setFont (mc2gui::silkFont (16.0f));
    g.drawText ("JTEKK AUDIO", 26, 8 + kTopBarH, 220, 22, Justification::centredLeft);

    g.setFont (mc2gui::silkFont (13.0f));
    g.drawText ("MC-2   TWIN-TUBE VARI-MU MASTERING COMPRESSOR",
                0, 10 + kTopBarH, kWidth, 18, Justification::centred);

    g.setColour (mc2gui::silkDim);
    g.setFont (mc2gui::silkFont (9.0f));
    g.drawText (String::fromUTF8 ("BALANCED I/O   \xc2\xb7   120 V B+   \xc2\xb7   ALL-TUBE CLASS A"),
                kWidth - 360 - 26, 12 + kTopBarH, 360, 14, Justification::centredRight);

    g.setColour (mc2gui::silkDim.withAlpha (0.5f));
    g.drawHorizontalLine (38 + kTopBarH, 20.0f, (float) kWidth - 20.0f);

    // centre block captions
    g.setColour (mc2gui::silkDim);
    g.setFont (mc2gui::silkFont (8.5f));
    g.drawText ("VARIABLE-GAIN TWIN TRIODES", 462, 182 + kTopBarH, 256, 11, Justification::centred);
    g.setColour (mc2gui::silk);
    g.setFont (mc2gui::silkFont (10.0f));
    g.drawText ("HARD-WIRE", kWidth / 2 - 70, 197 + kTopBarH, 140, 11, Justification::centred);

    // power jewel
    {
        const Point<float> lamp (438.0f, 244.0f + (float) kTopBarH);
        ColourGradient gl (mc2gui::accentRed.brighter (0.6f), lamp.x, lamp.y,
                           mc2gui::accentRed.withAlpha (0.0f), lamp.x + 16.0f, lamp.y + 16.0f, true);
        gl.isRadial = true;
        g.setGradientFill (gl);
        g.fillEllipse (Rectangle<float> (30.0f, 30.0f).withCentre (lamp));
        g.setColour (mc2gui::accentRed.brighter (0.25f));
        g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (lamp));
        g.setColour (Colour (0xff10141a));
        g.drawEllipse (Rectangle<float> (12.5f, 12.5f).withCentre (lamp), 1.5f);
        g.setColour (mc2gui::silkDim);
        g.setFont (mc2gui::silkFont (7.5f));
        g.drawText ("POWER", (int) lamp.x - 30, (int) lamp.y + 12, 60, 10, Justification::centred);
    }

    // section rules
    g.setColour (mc2gui::silkDim.withAlpha (0.35f));
    g.drawHorizontalLine (kMainRowY - 22, 20.0f, (float) kWidth - 20.0f);
    g.drawHorizontalLine (kLowerRowY - 18, 20.0f, (float) kWidth - 20.0f);

    // station titles
    g.setColour (mc2gui::silk);
    g.setFont (mc2gui::silkFont (10.5f));
    for (int i = 0; i < 8; ++i)
    {
        auto st = mainStation (i);
        g.drawText (mainTitles[i], st.getX(), kMainRowY - 18, st.getWidth(), 13,
                    Justification::centred);
    }
    g.setFont (mc2gui::silkFont (9.5f));
    for (int i = 0; i < 7; ++i)
    {
        auto st = lowerStation (i);
        g.drawText (lowerTitles[i], st.getX(), kLowerRowY - 14, st.getWidth(), 12,
                    Justification::centred);
    }

    // captions under the rotary switches
    g.setColour (mc2gui::silkDim);
    g.setFont (mc2gui::silkFont (7.5f));
    const char* switchCaptions[3] = { "SECONDS", "CIRCUIT", "CURVE" };
    const int switchStations[3]   = { 3, 5, 6 };
    for (int i = 0; i < 3; ++i)
    {
        auto st = mainStation (switchStations[i]);
        g.drawText (switchCaptions[i], st.getX(), kMainRowY + kMainRowH - 12,
                    st.getWidth(), 10, Justification::centred);
    }

    // serial plate
    g.setColour (mc2gui::silkDim);
    g.setFont (mc2gui::silkFont (8.0f));
    g.drawFittedText ("SIX-RECTIFIER PALETTE\nFOUR SIDECHAIN CURVES\nNo. 00002",
                      lowerStation (7).withY (kLowerRowY + 14).withHeight (56),
                      Justification::centred, 3);

    // corner screws (panel corners, not the window's - the preset bar sits above)
    for (auto c : { Point<float> (14.0f, 14.0f + (float) kTopBarH),
                    Point<float> ((float) kWidth - 14.0f, 14.0f + (float) kTopBarH),
                    Point<float> (14.0f, (float) kHeight - 14.0f),
                    Point<float> ((float) kWidth - 14.0f, (float) kHeight - 14.0f) })
    {
        g.setColour (Colour (0xff4a525c));
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (c));
        g.setColour (Colour (0xff181d23));
        g.drawLine (c.x - 3.0f, c.y, c.x + 3.0f, c.y, 1.4f);
    }
}

void MC2AudioProcessorEditor::timerCallback()
{
    // Collapse rapid knob-drag changes into one undo step roughly every
    // half second, rather than one step per audio-thread parameter tick.
    if (--undoTransactionCountdown <= 0)
    {
        undoTransactionCountdown = 15; // 15 ticks @ 30 Hz = 0.5 s
        proc.undoManager.beginNewTransaction();
    }
    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());

    const bool bypassed = pBypass != nullptr && pBypass->load() >= 0.5f;
    const bool outputMode = pMeterMode != nullptr && pMeterMode->load() >= 0.5f;
    const float cal[2] = { pCalL != nullptr ? pCalL->load() : 0.0f,
                           pCalR != nullptr ? pCalR->load() : 0.0f };

    float grSum = 0.0f;
    VUMeter* meters[2] = { &meterL, &meterR };

    for (int i = 0; i < 2; ++i)
    {
        const float gr  = proc.meterGrDB[i].load();
        const float rms = proc.meterOutRms[i].load();
        grSum += gr;

        float vu;
        if (outputMode)
            vu = 20.0f * std::log10 (rms + 1.0e-9f) + 21.0f + cal[i]; // 0 VU = -18 dBFS sine
        else
            vu = (bypassed ? 0.0f : -gr) + cal[i];

        meters[i]->setTarget (vu, outputMode ? VUMeter::Mode::Output
                                             : VUMeter::Mode::GainReduction);
    }

    tubeWindow.setGlow (bypassed ? 0.15f : grSum * 0.5f / 12.0f);
}
