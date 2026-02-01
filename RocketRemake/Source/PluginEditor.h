#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel/RocketLookAndFeel.h"

class TheRocketAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit TheRocketAudioProcessorEditor (TheRocketAudioProcessor&);
    ~TheRocketAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    TheRocketAudioProcessor& processor;
    RocketLookAndFeel rocketLnf;
    juce::Image backgroundImg;
    juce::Image cloudsImg;
    juce::Image panelImg;
    juce::Image rocketImg;
    juce::Image flameImg;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> uiAmountSmoothed;

    juce::ComponentBoundsConstrainer boundsConstrainer;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    juce::Slider amountKnob;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" };
    juce::TextButton nextButton { ">" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;

    // Internal developer panel for preset creation
    class DeveloperPanel : public juce::Component
    {
    public:
        explicit DeveloperPanel(TheRocketAudioProcessorEditor& editorIn);
        void resized() override;
        void rebuildParameterUI();

    private:
        TheRocketAudioProcessorEditor& editor;
        TheRocketAudioProcessor& processor;
        juce::ListBox moduleList;
        juce::Label moduleLabel;
        juce::TextButton moveUp { "Up" };
        juce::TextButton moveDown { "Down" };
        std::unique_ptr<juce::ListBoxModel> model;

        juce::ComboBox presetList;
        juce::TextEditor presetName;
        juce::Label presetLabel;
        juce::TextButton presetRefresh { "Refresh" };
        juce::TextButton presetSave { "Save" };
        juce::TextButton presetSaveAs { "Save As" };
        juce::TextButton presetDelete { "Delete" };

        juce::Viewport paramViewport;
        juce::Component paramContent;
        juce::OwnedArray<juce::Slider> sliders;
        juce::OwnedArray<juce::ToggleButton> toggles;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;

        juce::ComboBox assignParam;
        juce::Slider assignAmount;
        juce::ToggleButton assignUseRange { "Range" };
        juce::Slider assignMin;
        juce::Slider assignMax;
        juce::TextButton addAssign { "Add Map" };
        juce::TextButton removeAssign { "Remove Map" };
        juce::ListBox assignList;
        juce::Label modLabel;
        std::unique_ptr<juce::ListBoxModel> assignModel;

        void refreshPresetUi();
        void loadPresetByName(const juce::String& name);
        void saveCurrentAsName(const juce::String& name);

        void rebuildModuleList();
    };

    juce::ToggleButton devToggle { "Internal" };
    std::unique_ptr<DeveloperPanel> devPanel;

    friend class DeveloperPanel;

    void refreshPresetsFromDisk();
    void loadPresetByName(const juce::String& name);

    void refreshPresetList();
    void loadPresetFromIndex(int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheRocketAudioProcessorEditor)
};
