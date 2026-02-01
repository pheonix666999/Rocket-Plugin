#pragma once

#include <JuceHeader.h>
#include "DSP/FxChain.h"
#include "DSP/ModMatrix.h"

class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& state, FxChain& chain, ModMatrix& mod);

    void savePreset(const juce::String& name);
    void loadPreset(const juce::String& name);
    bool deletePreset(const juce::String& name);
    juce::StringArray getPresetNames() const;

    void appendState(juce::ValueTree& parent) const;
    void restoreFromState(const juce::ValueTree& parent);

private:
    juce::File getPresetFolder() const;
    void ensureFactoryPresets();
    void writePresetFileIfMissing(const juce::String& name, const juce::ValueTree& state);
    juce::ValueTree buildFactoryPresetState(const juce::String& name) const;
    void addFactoryPresetsIfMissing();

    static bool setParamValueInState(juce::ValueTree& state,
                                     const juce::String& paramID,
                                     float normalisedValue);

    juce::AudioProcessorValueTreeState& apvts;
    FxChain& fxChain;
    ModMatrix& modMatrix;
    juce::String currentPresetName;
};
