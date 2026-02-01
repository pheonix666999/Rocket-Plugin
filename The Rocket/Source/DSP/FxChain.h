#pragma once

#include <JuceHeader.h>
#include "Modules.h"

class FxChain
{
public:
    explicit FxChain(juce::AudioProcessorValueTreeState& state);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer,
                 juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& amount,
                 ModMatrix& modMatrix,
                 const FxTransportInfo& transport);

    void reset();

    void moveModule(int fromIndex, int toIndex);
    juce::StringArray getModuleOrder() const;
    void setModuleOrder(const juce::StringArray& order);

    void appendState(juce::ValueTree& parent) const;
    void restoreFromState(const juce::ValueTree& parent);

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);
    static void addParameterIDs(juce::StringArray& ids);

private:
    juce::AudioProcessorValueTreeState& apvts;
    struct ModuleEntry
    {
        std::unique_ptr<FxModule> module;
        ModuleKind kind;
    };

    juce::OwnedArray<ModuleEntry> modules;
    juce::StringArray order; // effect-only order

    juce::AudioBuffer<float> genBuffer;
    juce::AudioBuffer<float> dryGenBuffer;

    void buildDefaultOrder();
    ModuleEntry* findModuleById(const juce::String& id) const;
};
