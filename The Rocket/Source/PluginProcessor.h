#pragma once

#include <JuceHeader.h>
#include "DSP/FxChain.h"
#include "DSP/ModMatrix.h"
#include "DSP/PresetManager.h"
#include <atomic>

class TheRocketAudioProcessor : public juce::AudioProcessor
{
public:
    TheRocketAudioProcessor();
    ~TheRocketAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #if ! JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    ModMatrix& getModMatrix() { return modMatrix; }
    PresetManager& getPresetManager() { return presetManager; }
    FxChain& getFxChain() { return fxChain; }
    const juce::StringArray& getParameterIDs() const { return paramIDs; }

    float getSmoothedAmount() const { return amountSmoothed.getCurrentValue(); }

    // Pop-guard for preset switching: applies a short fade-out/fade-in on the output.
    void notifyPresetLoaded() noexcept { presetPopGuardSamples.store(kPresetPopGuardTotalSamples, std::memory_order_release); }

private:
    static constexpr int kPresetPopGuardHalfSamples = 256;
    static constexpr int kPresetPopGuardTotalSamples = kPresetPopGuardHalfSamples * 2;

    juce::AudioProcessorValueTreeState apvts;
    FxChain fxChain;
    ModMatrix modMatrix;
    PresetManager presetManager;

    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> globalMixSmoothed;
    juce::StringArray paramIDs;

    std::atomic<int> presetPopGuardSamples { 0 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheRocketAudioProcessor)
};
