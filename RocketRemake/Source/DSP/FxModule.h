#pragma once

#include <JuceHeader.h>

class ModMatrix; // Forward declaration

enum class ModuleKind { Effect, Generator };

struct FxTransportInfo
{
    double bpm = 120.0;
    bool isPlaying = false;
    int64_t ppqPosition = 0;
    double sampleRate = 44100.0;
};

class FxModule
{
public:
    FxModule(juce::AudioProcessorValueTreeState& state, const juce::String& moduleId, ModuleKind kindIn)
        : apvts(state), moduleID(moduleId), kind(kindIn) {}

    virtual ~FxModule() = default;

    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    virtual void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) = 0;

    const juce::String& getId() const { return moduleID; }
    ModuleKind getKind() const { return kind; }

    bool isEnabled() const
    {
        if (auto* p = apvts.getRawParameterValue(moduleID + "_enabled"))
            return p->load() > 0.5f;
        return true;
    }

    float getMix(ModMatrix& modMatrix) const;

protected:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String moduleID;
    ModuleKind kind;
};
