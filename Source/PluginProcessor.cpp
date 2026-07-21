#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
// Exact inverse pair (mid=(L+R)/2, side=(L-R)/2 <-> L=mid+side, R=mid-side):
// applied around the whole oversampled path so the engine's two channels
// carry Mid/Side instead of Left/Right when the mode is on.
void encodeMidSide (juce::AudioBuffer<float>& buffer, int numCh, int n)
{
    if (numCh != 2)
        return;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float mid  = 0.5f * (l[i] + r[i]);
        const float side = 0.5f * (l[i] - r[i]);
        l[i] = mid;
        r[i] = side;
    }
}

void decodeMidSide (juce::AudioBuffer<float>& buffer, int numCh, int n)
{
    if (numCh != 2)
        return;

    float* m = buffer.getWritePointer (0);
    float* s = buffer.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float mid = m[i], side = s[i];
        m[i] = mid + side;
        s[i] = mid - side;
    }
}
} // namespace

MC2AudioProcessor::MC2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "MC2", createParameterLayout())
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
    pOversampling = apvts.getRawParameterValue (ParamID::oversampling);
    pMsMode    = apvts.getRawParameterValue (ParamID::msMode);

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamID::bypass));
}

int MC2AudioProcessor::getNumPrograms()
{
    // Program 0 is "Default" (whatever the current knob settings are);
    // programs 1..N are the factory presets below it.
    return 1 + (int) mc2presets::factoryPresets().size();
}

const juce::String MC2AudioProcessor::getProgramName (int index)
{
    if (index <= 0)
        return "Default";

    const auto& presets = mc2presets::factoryPresets();
    if (index - 1 < (int) presets.size())
        return presets[(size_t) (index - 1)].name;

    return {};
}

void MC2AudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = mc2presets::factoryPresets();
    if (index < 0 || index > (int) presets.size())
        return;

    currentProgramIndex = index;

    if (index == 0)
        return; // "Default" leaves whatever is currently dialled in alone

    const auto& preset = presets[(size_t) (index - 1)];
    undoManager.beginNewTransaction ("Load preset: " + juce::String (preset.name));
    mc2presets::apply (apvts, preset);
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

    hostSampleRate = sampleRate;
    hostBlockSize  = samplesPerBlock;

    // All 4 factors (1x/2x/4x/8x) are preallocated up front: the OVERSAMPLING
    // control can switch between them at any time, not just between host
    // prepareToPlay calls, and preallocating keeps that switch allocation-free
    // (see applyOversamplingIndex()). Linear-phase FIR halfbands throughout;
    // latency is reported to the host whenever the active factor changes.
    for (int i = 0; i < (int) oversamplers.size(); ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            numCh, (size_t) i,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true, false);
        oversamplers[(size_t) i]->initProcessing (static_cast<size_t> (samplesPerBlock));
    }

    // Loudness/true-peak metering runs on the real output rate, not the
    // engine's internal oversampled rate.
    loudnessMeter.prepare (sampleRate, (int) numCh);

    const int idx = pOversampling != nullptr
                       ? juce::jlimit (0, (int) oversamplers.size() - 1,
                                       static_cast<int> (pOversampling->load() + 0.5f))
                       : activeOversamplingIdx;
    applyOversamplingIndex (idx);
}

void MC2AudioProcessor::releaseResources()
{
    for (auto& ovs : oversamplers)
        if (ovs != nullptr)
            ovs->reset();
    engine.reset();
    loudnessMeter.reset();
}

void MC2AudioProcessor::applyOversamplingIndex (int idx)
{
    activeOversamplingIdx = idx;
    const int factor = 1 << idx; // 1, 2, 4, 8

    // MC2Engine::prepare() never allocates (fixed-size channel state), so
    // this is safe to call here even when idx changed mid-stream, from
    // inside processBlock.
    engine.prepare (hostSampleRate * factor, hostBlockSize * factor);
    updateEngineParams();

    setLatencySamples (juce::roundToInt (
        oversamplers[(size_t) activeOversamplingIdx]->getLatencyInSamples()));
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

    if (n == 0 || numCh == 0 || oversamplers[(size_t) activeOversamplingIdx] == nullptr)
        return;

    if (pOversampling != nullptr)
    {
        const int desiredIdx = juce::jlimit (0, (int) oversamplers.size() - 1,
                                             static_cast<int> (pOversampling->load() + 0.5f));
        if (desiredIdx != activeOversamplingIdx)
            applyOversamplingIndex (desiredIdx);
    }

    auto& ovs = *oversamplers[(size_t) activeOversamplingIdx];

    const bool bypassed = bypassParam != nullptr && bypassParam->get();

    if (bypassed)
    {
        // Hard-wire bypass: the programme never sees the circuit. Push it
        // through the (transparent) halfband pair only so the path delay
        // matches and toggling stays seamless; meters keep reading the line.
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            static_cast<size_t> (numCh),
                                            static_cast<size_t> (n));
        auto up = ovs.processSamplesUp (block);
        juce::ignoreUnused (up);
        ovs.processSamplesDown (block);

        for (int c = 0; c < 2; ++c)
        {
            const int src = juce::jmin (c, numCh - 1);
            meterGrDB[c].store (0.0f);
            meterOutRms[c].store (buffer.getRMSLevel (src, 0, n));
        }
        updateMeters (buffer, numCh, n);
        return;
    }

    updateEngineParams();

    const bool midSide = numCh == 2 && pMsMode != nullptr && pMsMode->load() >= 0.5f;
    if (midSide)
        encodeMidSide (buffer, numCh, n);

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                        static_cast<size_t> (numCh),
                                        static_cast<size_t> (n));
    auto up = ovs.processSamplesUp (block);

    float* chans[2] = { nullptr, nullptr };
    for (int c = 0; c < numCh; ++c)
        chans[c] = up.getChannelPointer (static_cast<size_t> (c));

    engine.process (chans, numCh, static_cast<int> (up.getNumSamples()));

    ovs.processSamplesDown (block);

    if (midSide)
        decodeMidSide (buffer, numCh, n);

    for (int c = 0; c < 2; ++c)
    {
        const int src = juce::jmin (c, numCh - 1);
        meterGrDB[c].store (engine.getGainReductionDB (src));
        meterOutRms[c].store (engine.getOutputRms (src));
    }
    updateMeters (buffer, numCh, n);
}

void MC2AudioProcessor::updateMeters (const juce::AudioBuffer<float>& buffer, int numCh, int n)
{
    const float* chans[2] = { nullptr, nullptr };
    for (int c = 0; c < numCh; ++c)
        chans[c] = buffer.getReadPointer (c);

    loudnessMeter.process (chans, numCh, n);
    meterLufsI.store (loudnessMeter.getIntegratedLUFS());
    meterLufsS.store (loudnessMeter.getShortTermLUFS());
    meterTruePeakDB.store (loudnessMeter.getTruePeakDB());

    int pos = scopeWritePos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int c = 0; c < numCh; ++c)
            mono += chans[c][i];
        mono /= (float) numCh;

        scopeBuffer[(size_t) pos] = mono;
        pos = (pos + 1) % kScopeSize;
    }
    scopeWritePos.store (pos, std::memory_order_relaxed);
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

    // A saved session can restore a non-default oversampling factor; if
    // prepareToPlay already ran (the common host order: prepare, then
    // restore state), re-sync now so the reported latency is right from
    // the very first block instead of only catching up lazily inside the
    // next processBlock() call.
    if (pOversampling != nullptr && oversamplers[(size_t) activeOversamplingIdx] != nullptr)
    {
        const int idx = juce::jlimit (0, (int) oversamplers.size() - 1,
                                      static_cast<int> (pOversampling->load() + 0.5f));
        if (idx != activeOversamplingIdx)
            applyOversamplingIndex (idx);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MC2AudioProcessor();
}
