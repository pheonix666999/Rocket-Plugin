#include "ModMatrix.h"

ModMatrix::ModMatrix(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
}

void ModMatrix::prepare(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void ModMatrix::setMacroValue(float macro)
{
    macroValue = juce::jlimit(0.0f, 1.0f, macro);
}

float ModMatrix::getModulatedParamValue(const juce::String& paramID, float baseValue) const
{
    float modded = baseValue;

    for (const auto& a : assignments)
    {
        if (a.paramID == paramID)
        {
            if (a.useRange)
            {
                // Interpolate between min and max based on macro and amount/direction
                float t = macroValue * a.amount;
                if (a.amount >= 0.0f)
                    modded = a.min + t * (a.max - a.min);
                else
                    modded = a.max + (-t) * (a.min - a.max);
            }
            else
            {
                // Direct modulation: add amount * macro to base value
                modded = baseValue + a.amount * macroValue;
            }
            break;
        }
    }

    return modded;
}

void ModMatrix::addAssignment(const Assignment& a)
{
    // Remove any existing assignment for the same param
    for (int i = assignments.size(); --i >= 0;)
    {
        if (assignments.getReference(i).paramID == a.paramID)
            assignments.remove(i);
    }
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
    juce::ValueTree modTree("MODMATRIX");

    for (const auto& a : assignments)
    {
        juce::ValueTree asg("ASSIGNMENT");
        asg.setProperty("paramID", a.paramID, nullptr);
        asg.setProperty("amount", a.amount, nullptr);
        asg.setProperty("useRange", a.useRange, nullptr);
        asg.setProperty("min", a.min, nullptr);
        asg.setProperty("max", a.max, nullptr);
        modTree.addChild(asg, -1, nullptr);
    }

    parent.addChild(modTree, -1, nullptr);
}

void ModMatrix::restoreFromState(const juce::ValueTree& parent)
{
    auto modTree = parent.getChildWithName("MODMATRIX");
    if (!modTree.isValid())
        return;

    assignments.clear();

    for (int i = 0; i < modTree.getNumChildren(); ++i)
    {
        auto asg = modTree.getChild(i);
        if (asg.hasType("ASSIGNMENT"))
        {
            Assignment a;
            a.paramID = asg.getProperty("paramID").toString();
            a.amount = (float)(double)asg.getProperty("amount", 0.0);
            a.useRange = (bool)asg.getProperty("useRange", false);
            a.min = (float)(double)asg.getProperty("min", 0.0);
            a.max = (float)(double)asg.getProperty("max", 1.0);
            assignments.add(a);
        }
    }
}

void ModMatrix::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& /*layout*/)
{
    // The macro "amount" parameter is added in the main parameter layout
}
