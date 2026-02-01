#include "ModMatrix.h"

ModMatrix::ModMatrix(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
}

void ModMatrix::prepare(double, int) {}

void ModMatrix::setMacroValue(float macro)
{
    macroValue = juce::jlimit(0.0f, 1.0f, macro);
}

float ModMatrix::getModulatedParamValue(const juce::String& paramID, float baseValue) const
{
    auto* param = apvts.getParameter(paramID);
    if (param == nullptr)
        return baseValue;

    auto range = param->getNormalisableRange();
    float norm = range.convertTo0to1(baseValue);

    for (const auto& a : assignments)
    {
        if (a.paramID != paramID)
            continue;

        const float depth = juce::jlimit(0.0f, 1.0f, std::abs(a.amount));
        const float direction = (a.amount >= 0.0f) ? 1.0f : -1.0f;

        if (a.useRange)
        {
            float minV = a.min;
            float maxV = a.max;
            if (direction < 0.0f)
                std::swap(minV, maxV);

            const float targetValue = juce::jmap(macroValue, minV, maxV);
            const float targetNorm = range.convertTo0to1(targetValue);
            norm = norm + depth * (targetNorm - norm);
        }
        else
        {
            norm = juce::jlimit(0.0f, 1.0f, norm + direction * depth * macroValue);
        }
    }

    return range.convertFrom0to1(norm);
}

void ModMatrix::addAssignment(const Assignment& a)
{
    assignments.add(a);
}

void ModMatrix::removeAssignment(int index)
{
    if (juce::isPositiveAndBelow(index, assignments.size()))
        assignments.remove(index);
}

void ModMatrix::clear()
{
    assignments.clear();
}

void ModMatrix::appendState(juce::ValueTree& parent) const
{
    juce::ValueTree modTree("MOD_MATRIX");
    for (const auto& a : assignments)
    {
        juce::ValueTree child("ASSIGN");
        child.setProperty("paramID", a.paramID, nullptr);
        child.setProperty("amount", a.amount, nullptr);
        child.setProperty("useRange", a.useRange, nullptr);
        child.setProperty("min", a.min, nullptr);
        child.setProperty("max", a.max, nullptr);
        modTree.addChild(child, -1, nullptr);
    }
    parent.addChild(modTree, -1, nullptr);
}

void ModMatrix::restoreFromState(const juce::ValueTree& parent)
{
    assignments.clear();
    auto modTree = parent.getChildWithName("MOD_MATRIX");
    if (!modTree.isValid())
        return;

    for (int i = 0; i < modTree.getNumChildren(); ++i)
    {
        auto child = modTree.getChild(i);
        Assignment a;
        a.paramID = child.getProperty("paramID").toString();
        a.amount = (float) child.getProperty("amount");
        a.useRange = (bool) child.getProperty("useRange");
        a.min = (float) child.getProperty("min");
        a.max = (float) child.getProperty("max");
        assignments.add(a);
    }
}

void ModMatrix::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout&)
{
    // No direct parameters; stored in ValueTree for presets.
}
