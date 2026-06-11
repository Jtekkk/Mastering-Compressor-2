#include "PluginProcessor.h"
#include "PluginEditor.h"

MC2AudioProcessor::MC2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MC2", createParameterLayout())
{
    pInput     = apvts.getRawParameterValue (ParamID::input);
    pThreshold = apvts.getRawParameterValue (ParamID::threshold);
    pAttack    = apvts.getRawParameterValue (ParamID::attack);
    pRecovery  = apvts.getRawParameterValue (ParamID::recovery);
    pMode      = apvts.getRawParameterValue (ParamID::mode);
    pRectifier = apvts.getRawParameterValue (ParamID::rectifier);
    pScEq      = apvts.getRawParameterValue (ParamID::scEq);
    pLink      = apvts.getRawParameterValue (ParamID::link);
    pOutput    = apvts.getRawParameterValue (ParamID::output);
    pEqIn      = apvts.getRawParameterValue (ParamID::eqIn);
    pEqLow     = apvts.getRawParameterValue (ParamID::eqLow);
    pEqAir     = apvts.getRawParameterValue (ParamID::eqAir);

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamID::bypass));
}

bool MC2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void MC2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = static_cast<size_t> (juce::jmax (1, getTotalNumOutputChannels()));

    // The whole signal path runs 2x oversampled: the tube curvature stays
    // alias-free and the detector resolution doubles. Linear-phase FIR
    // halfbands, latency reported to the host.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        numCh, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true, false);
    oversampler->initProcessing (static_cast<size_t> (samplesPerBlock));

    engine.prepare (sampleRate * 2.0, samplesPerBlock * 2);
    updateEngineParams();

    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));
}

void MC2AudioProcessor::releaseResources()
{
    if (oversampler != nullptr)
        oversampler->reset();
    engine.reset();
}

void MC2AudioProcessor::updateEngineParams()
{
    mc2::EngineParams p;
    p.inputGainDB  = pInput->load();
    p.thresholdDB  = pThreshold->load();
    p.attackMs     = pAttack->load();
    p.recoveryIdx  = static_cast<int> (pRecovery->load() + 0.5f);
    p.rectifierIdx = static_cast<int> (pRectifier->load() + 0.5f);
    p.scEqIdx      = static_cast<int> (pScEq->load() + 0.5f);
    p.limitMode    = pMode->load() >= 0.5f;
    p.stereoLink   = pLink->load() >= 0.5f;
    p.eqIn         = pEqIn->load() >= 0.5f;
    p.eqLowDB      = pEqLow->load();
    p.eqAirDB      = pEqAir->load();
    p.outGainDB    = pOutput->load();
    engine.setParams (p);
}

void MC2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (2, buffer.getNumChannels(), getTotalNumOutputChannels());
    const int n     = buffer.getNumSamples();

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    if (n == 0 || numCh == 0 || oversampler == nullptr)
        return;

    const bool bypassed = bypassParam != nullptr && bypassParam->get();

    if (bypassed)
    {
        // Hard-wire bypass: the programme never sees the circuit. Push it
        // through the (transparent) halfband pair only so the path delay
        // matches and toggling stays seamless; meters keep reading the line.
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            static_cast<size_t> (numCh),
                                            static_cast<size_t> (n));
        auto up = oversampler->processSamplesUp (block);
        juce::ignoreUnused (up);
        oversampler->processSamplesDown (block);

        for (int c = 0; c < 2; ++c)
        {
            const int src = juce::jmin (c, numCh - 1);
            meterGrDB[c].store (0.0f);
            meterOutRms[c].store (buffer.getRMSLevel (src, 0, n));
        }
        return;
    }

    updateEngineParams();

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                        static_cast<size_t> (numCh),
                                        static_cast<size_t> (n));
    auto up = oversampler->processSamplesUp (block);

    float* chans[2] = { nullptr, nullptr };
    for (int c = 0; c < numCh; ++c)
        chans[c] = up.getChannelPointer (static_cast<size_t> (c));

    engine.process (chans, numCh, static_cast<int> (up.getNumSamples()));

    oversampler->processSamplesDown (block);

    for (int c = 0; c < 2; ++c)
    {
        const int src = juce::jmin (c, numCh - 1);
        meterGrDB[c].store (engine.getGainReductionDB (src));
        meterOutRms[c].store (engine.getOutputRms (src));
    }
}

juce::AudioProcessorEditor* MC2AudioProcessor::createEditor()
{
    return new MC2AudioProcessorEditor (*this);
}

void MC2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MC2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MC2AudioProcessor();
}
