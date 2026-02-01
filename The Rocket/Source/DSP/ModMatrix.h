#pragma once

#include <JuceHeader.h>

class ModMatrix
{
public:
    struct Assignment
    {
        juce::String paramID;
        float amount = 0.0f; // -1..1
        bool useRange = false;
        float min = 0.0f;
        float max = 1.0f;
    };

    explicit ModMatrix(juce::AudioProcessorValueTreeState& state);

    void prepare(double sampleRate, int samplesPerBlock);
    void setMacroValue(float macro);

    float getModulatedParamValue(const juce::String& paramID, float baseValue) const;

    void addAssignment(const Assignment& a);
    void removeAssignment(int index);
    void clear();

    const juce::Array<Assignment>& getAssignments() const { return assignments; }

    void appendState(juce::ValueTree& parent) const;
    void restoreFromState(const juce::ValueTree& parent);

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    juce::AudioProcessorValueTreeState& apvts;
    float macroValue = 0.0f;
    juce::Array<Assignment> assignments;
};
