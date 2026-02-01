#include "PluginProcessor.h"
#include "PluginEditor.h"

TheRocketAudioProcessor::TheRocketAudioProcessor()
    : AudioProcessor (BusesProperties()
      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      fxChain(apvts),
      modMatrix(apvts),
      presetManager(apvts, fxChain, modMatrix)
{
    // Build parameter ID list
    FxChain::addParameterIDs(paramIDs);
}

TheRocketAudioProcessor::~TheRocketAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout TheRocketAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Main Amount macro (the only user-visible control)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "amount", "Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f));

    // Add all FX chain parameters
    FxChain::addParameters(layout);

    return layout;
}

const juce::String TheRocketAudioProcessor::getName() const { return JucePlugin_Name; }
bool TheRocketAudioProcessor::acceptsMidi() const { return false; }
bool TheRocketAudioProcessor::producesMidi() const { return false; }
bool TheRocketAudioProcessor::isMidiEffect() const { return false; }
double TheRocketAudioProcessor::getTailLengthSeconds() const { return 2.0; }
int TheRocketAudioProcessor::getNumPrograms() { return 1; }
int TheRocketAudioProcessor::getCurrentProgram() { return 0; }
void TheRocketAudioProcessor::setCurrentProgram(int) {}
const juce::String TheRocketAudioProcessor::getProgramName(int) { return {}; }
void TheRocketAudioProcessor::changeProgramName(int, const juce::String&) {}

void TheRocketAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    fxChain.prepare(spec);
    modMatrix.prepare(sampleRate, samplesPerBlock);

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);

    amountSmoothed.reset(sampleRate, 0.05); // 50ms smoothing
    globalMixSmoothed.reset(sampleRate, 0.02); // 20ms smoothing
}

void TheRocketAudioProcessor::releaseResources()
{
}

#if ! JucePlugin_PreferredChannelConfigurations
bool TheRocketAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void TheRocketAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear unused channels
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Get current Amount value
    const float amountTarget = apvts.getRawParameterValue("amount")->load();
    amountSmoothed.setTargetValue(amountTarget);

    // Get global mix
    float globalMixTarget = 1.0f;
    if (auto* p = apvts.getRawParameterValue("global_mix"))
        globalMixTarget = modMatrix.getModulatedParamValue("global_mix", p->load());
    globalMixSmoothed.setTargetValue(globalMixTarget);

    // Copy dry signal for mix
    dryBuffer.makeCopyOf(buffer, true);

    // Get transport info for sync
    FxTransportInfo transport;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                transport.bpm = *bpm;
            transport.isPlaying = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
                transport.ppqPosition = (int64_t)(*ppq * 1000.0);
        }
    }
    transport.sampleRate = getSampleRate();

    // Update macro value
    modMatrix.setMacroValue(amountSmoothed.getCurrentValue());

    // Process FX chain
    fxChain.process(buffer, amountSmoothed, modMatrix, transport);

    // Apply global mix (dry/wet)
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        float* wet = buffer.getWritePointer(ch);
        const float* dry = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = globalMixSmoothed.getNextValue();
            wet[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
        }
    }

    // Apply pop-guard fade for preset switching
    int remaining = presetPopGuardSamples.load(std::memory_order_acquire);
    if (remaining > 0)
    {
        const int toProcess = juce::jmin(remaining, numSamples);
        
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < toProcess; ++i)
            {
                const int sample = remaining - i;
                float gain;
                
                if (sample > kPresetPopGuardHalfSamples)
                {
                    // Fade out phase
                    gain = (float)(sample - kPresetPopGuardHalfSamples) / (float)kPresetPopGuardHalfSamples;
                }
                else
                {
                    // Fade in phase
                    gain = 1.0f - (float)sample / (float)kPresetPopGuardHalfSamples;
                }
                
                data[i] *= gain;
            }
        }
        
        presetPopGuardSamples.store(remaining - toProcess, std::memory_order_release);
    }

    // Advance smoother
    for (int i = 0; i < numSamples; ++i)
        amountSmoothed.getNextValue();
}

bool TheRocketAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TheRocketAudioProcessor::createEditor()
{
    return new TheRocketAudioProcessorEditor(*this);
}

void TheRocketAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    
    // Add FX chain state
    fxChain.appendState(state);
    
    // Add modulation matrix state
    modMatrix.appendState(state);
    
    // Add preset manager state
    presetManager.appendState(state);

    auto xml = state.createXml();
    if (xml)
        copyXmlToBinary(*xml, destData);
}

void TheRocketAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml)
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid() && state.hasType(apvts.state.getType()))
        {
            apvts.replaceState(state);
            fxChain.restoreFromState(state);
            modMatrix.restoreFromState(state);
            presetManager.restoreFromState(state);
        }
    }
}

// Plugin filter is created in Main.cpp
