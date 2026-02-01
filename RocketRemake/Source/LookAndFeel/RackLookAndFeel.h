#pragma once

#include <JuceHeader.h>

class RackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha(0.45f));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white.withAlpha(0.85f));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.6f));
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.5f));
    }
};

