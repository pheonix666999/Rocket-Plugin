#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kAmountSmoothingSeconds = 0.05f;
    juce::StringArray& parameterIdCache()
    {
        static juce::StringArray ids;
        return ids;
    }
}

TheRocketAudioProcessor::TheRocketAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      fxChain(apvts),
      modMatrix(apvts),
      presetManager(apvts, fxChain, modMatrix)
{
    amountSmoothed.reset(44100.0, kAmountSmoothingSeconds);
    globalMixSmoothed.reset(44100.0, 0.05);
    paramIDs = parameterIdCache();
}

TheRocketAudioProcessor::~TheRocketAudioProcessor() = default;

const juce::String TheRocketAudioProcessor::getName() const { return JucePlugin_Name; }

bool TheRocketAudioProcessor::acceptsMidi() const { return false; }
bool TheRocketAudioProcessor::producesMidi() const { return false; }
bool TheRocketAudioProcessor::isMidiEffect() const { return false; }

double TheRocketAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int TheRocketAudioProcessor::getNumPrograms() { return 1; }
int TheRocketAudioProcessor::getCurrentProgram() { return 0; }
void TheRocketAudioProcessor::setCurrentProgram (int) {}
const juce::String TheRocketAudioProcessor::getProgramName (int) { return {}; }
void TheRocketAudioProcessor::changeProgramName (int, const juce::String&) {}

void TheRocketAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getTotalNumOutputChannels() };
    fxChain.prepare(spec);
    modMatrix.prepare(sampleRate, samplesPerBlock);

    amountSmoothed.reset(sampleRate, kAmountSmoothingSeconds);
    amountSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("amount")->load());

    globalMixSmoothed.reset(sampleRate, 0.05);
    globalMixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("global_mix")->load());

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
}

void TheRocketAudioProcessor::releaseResources() {}

bool TheRocketAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

void TheRocketAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    dryBuffer.makeCopyOf(buffer, true);

    // Smooth Amount macro
    const auto amountTarget = apvts.getRawParameterValue("amount")->load();
    amountSmoothed.setTargetValue(amountTarget);

    const float macroStart = amountSmoothed.getNextValue();
    if (buffer.getNumSamples() > 1)
        amountSmoothed.skip(buffer.getNumSamples() - 1);
    const float macroEnd = amountSmoothed.getCurrentValue();
    const float macro = 0.5f * (macroStart + macroEnd);
    modMatrix.setMacroValue(macro);

    FxTransportInfo transport;
    if (auto* audioPlayHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo info;
        if (audioPlayHead->getCurrentPosition(info))
        {
            transport.isPlaying = info.isPlaying;
            if (info.bpm > 0.0)
                transport.bpm = info.bpm;
        }
    }

    fxChain.process(buffer, amountSmoothed, modMatrix, transport);

    // Global mix
    const float globalMixTarget = modMatrix.getModulatedParamValue("global_mix", apvts.getRawParameterValue("global_mix")->load());
    globalMixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, globalMixTarget));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float mix = globalMixSmoothed.getNextValue();
            wet[i] = dry[i] + mix * (wet[i] - dry[i]);
        }
    }

    // Preset pop-guard: gentle fade-out then fade-in after a preset is loaded.
    int remaining = presetPopGuardSamples.load(std::memory_order_acquire);
    if (remaining > 0)
    {
        for (int i = 0; i < buffer.getNumSamples() && remaining > 0; ++i)
        {
            const int progressed = kPresetPopGuardTotalSamples - remaining;
            float g = 1.0f;
            if (progressed < kPresetPopGuardHalfSamples)
                g = 1.0f - (float) progressed / (float) kPresetPopGuardHalfSamples;
            else
                g = (float) (progressed - kPresetPopGuardHalfSamples) / (float) kPresetPopGuardHalfSamples;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer(ch)[i] *= g;

            --remaining;
        }
        presetPopGuardSamples.store(remaining, std::memory_order_release);
    }
}

bool TheRocketAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TheRocketAudioProcessor::createEditor()
{
    return new TheRocketAudioProcessorEditor (*this);
}

void TheRocketAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    presetManager.appendState(state);

    juce::MemoryOutputStream stream(destData, true);
    state.writeToStream(stream);
}

void TheRocketAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData(data, sizeInBytes);
    if (state.isValid())
    {
        apvts.replaceState(state);
        presetManager.restoreFromState(state);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TheRocketAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto& ids = parameterIdCache();
    ids.clear();

    auto addParam = [&](std::unique_ptr<juce::RangedAudioParameter> p)
    {
        ids.add(p->paramID);
        layout.add(std::move(p));
    };

    addParam(std::make_unique<juce::AudioParameterFloat>("amount", "Amount", 0.0f, 1.0f, 0.0f));
    addParam(std::make_unique<juce::AudioParameterFloat>("global_mix", "Global Mix", 0.0f, 1.0f, 1.0f));

    FxChain::addParameters(layout);
    ModMatrix::addParameters(layout);
    FxChain::addParameterIDs(ids);

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheRocketAudioProcessor();
}
