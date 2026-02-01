#include "PluginEditor.h"

namespace
{
    constexpr int kHeaderHeight = 60;
    constexpr int kPadding = 12;
    constexpr int kKnobSize = 140;

    struct MainUiLayout
    {
        float scale = 1.0f;
        juce::Rectangle<int> panel;
        juce::Rectangle<int> knob;
        juce::Rectangle<int> prev;
        juce::Rectangle<int> next;
        juce::Rectangle<int> preset;
        juce::Rectangle<int> devToggle;
        juce::Rectangle<int> animationArea;
    };

    MainUiLayout makeMainLayout(const juce::Rectangle<int>& bounds, const juce::Image& panelImg)
    {
        MainUiLayout l;
        const float refW = 600.0f;
        const float refH = 900.0f;
        l.scale = juce::jlimit(0.65f, 2.0f, juce::jmin(bounds.getWidth() / refW, bounds.getHeight() / refH));

        const int padding = juce::roundToInt(16.0f * l.scale);

        if (panelImg.isValid())
        {
            const float panelAspect = (float) panelImg.getWidth() / (float) panelImg.getHeight();

            const int maxPanelW = juce::jmax(1, bounds.getWidth() - padding * 2);
            const int maxPanelH = juce::jmax(1, juce::roundToInt(bounds.getHeight() * 0.29f));

            int panelW = juce::jmin(maxPanelW, juce::roundToInt(392.0f * l.scale));
            int panelH = juce::roundToInt(panelW / panelAspect);

            if (panelH > maxPanelH)
            {
                panelH = maxPanelH;
                panelW = juce::roundToInt(panelH * panelAspect);
            }

            panelW = juce::jmin(panelW, maxPanelW);
            panelH = juce::jmin(panelH, bounds.getHeight());

            const int panelX = bounds.getCentreX() - panelW / 2;
            const int panelY = bounds.getBottom() - padding - panelH;
            l.panel = { panelX, panelY, panelW, panelH };
        }
        else
        {
            const int panelH = juce::roundToInt(200.0f * l.scale);
            auto b = bounds;
            l.panel = b.removeFromBottom(panelH).reduced(padding);
        }

        l.animationArea = juce::Rectangle<int>(bounds.getX(), bounds.getY(), bounds.getWidth(),
                                               juce::jmax(0, l.panel.getY() - bounds.getY()));

        const int knobSize = juce::roundToInt(l.panel.getHeight() * 0.62f);
        l.knob = juce::Rectangle<int>(0, 0, knobSize, knobSize)
                     .withCentre({ l.panel.getCentreX(), l.panel.getY() + juce::roundToInt(l.panel.getHeight() * 0.48f) });

        const int btnSize = juce::jmax(18, juce::roundToInt(26.0f * l.scale));
        const int topInset = juce::roundToInt(l.panel.getHeight() * 0.06f);
        const int leftInset = juce::roundToInt(l.panel.getWidth() * 0.05f);
        const int gap = juce::roundToInt(6.0f * l.scale);

        l.prev = { l.panel.getX() + leftInset, l.panel.getY() + topInset, btnSize, btnSize };
        l.next = l.prev.translated(btnSize + gap, 0);

        const int presetW = juce::roundToInt(150.0f * l.scale);
        const int presetH = juce::jmax(btnSize, juce::roundToInt(28.0f * l.scale));
        l.preset = { l.next.getRight() + gap * 2, l.panel.getY() + topInset - juce::roundToInt(1.0f * l.scale), presetW, presetH };

        l.devToggle = { l.panel.getRight() - juce::roundToInt(70.0f * l.scale), l.panel.getY() + topInset, juce::roundToInt(65.0f * l.scale),
                        btnSize };

        return l;
    }
}

class ModuleListModel : public juce::ListBoxModel
{
public:
    ModuleListModel(TheRocketAudioProcessor& p, juce::ListBox& lb) : processor(p), list(lb) {}

    int getNumRows() override 
    { 
        try {
            return processor.getFxChain().getModuleOrder().size(); 
        } catch (...) {
            return 0;
        }
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowSelected) override
    {
        auto order = processor.getFxChain().getModuleOrder();
        if (!juce::isPositiveAndBelow(row, order.size()))
            return;

        auto name = order[row];
        g.fillAll(rowSelected ? juce::Colours::darkgrey : juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.drawText(name, 8, 0, width - 16, height, juce::Justification::centredLeft);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        list.selectRow(row);
    }

private:
    TheRocketAudioProcessor& processor;
    juce::ListBox& list;
};

class AssignmentListModel : public juce::ListBoxModel
{
public:
    AssignmentListModel(TheRocketAudioProcessor& p, juce::ListBox& lb) : processor(p), list(lb) {}

    int getNumRows() override 
    { 
        try {
            return processor.getModMatrix().getAssignments().size(); 
        } catch (...) {
            return 0;
        }
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowSelected) override
    {
        auto& assigns = processor.getModMatrix().getAssignments();
        if (!juce::isPositiveAndBelow(row, assigns.size()))
            return;

        const auto& a = assigns.getReference(row);
        g.fillAll(rowSelected ? juce::Colours::darkgrey : juce::Colours::black);
        g.setColour(juce::Colours::white);
        auto text = a.paramID + "  amt=" + juce::String(a.amount, 2);
        if (a.useRange)
            text += " range=" + juce::String(a.min, 2) + ":" + juce::String(a.max, 2);
        g.drawText(text, 8, 0, width - 16, height, juce::Justification::centredLeft);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        list.selectRow(row);
    }

private:
    TheRocketAudioProcessor& processor;
    juce::ListBox& list;
};

TheRocketAudioProcessorEditor::DeveloperPanel::DeveloperPanel(TheRocketAudioProcessorEditor& editorIn)
    : editor(editorIn), processor(editorIn.processor)
{
    addAndMakeVisible(moduleList);
    addAndMakeVisible(moduleLabel);
    addAndMakeVisible(moveUp);
    addAndMakeVisible(moveDown);
    addAndMakeVisible(paramViewport);
    addAndMakeVisible(presetLabel);
    addAndMakeVisible(presetList);
    addAndMakeVisible(presetName);
    addAndMakeVisible(presetRefresh);
    addAndMakeVisible(presetSave);
    addAndMakeVisible(presetSaveAs);
    addAndMakeVisible(presetDelete);
    addAndMakeVisible(assignParam);
    addAndMakeVisible(assignAmount);
    addAndMakeVisible(assignUseRange);
    addAndMakeVisible(assignMin);
    addAndMakeVisible(assignMax);
    addAndMakeVisible(addAssign);
    addAndMakeVisible(removeAssign);
    addAndMakeVisible(modLabel);
    addAndMakeVisible(assignList);

    paramViewport.setViewedComponent(&paramContent, false);

    model.reset(new ModuleListModel(processor, moduleList));
    moduleList.setModel(model.get());

    assignModel.reset(new AssignmentListModel(processor, assignList));
    assignList.setModel(assignModel.get());

    assignAmount.setSliderStyle(juce::Slider::LinearHorizontal);
    assignAmount.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    assignAmount.setRange(-1.0, 1.0, 0.01);
    assignAmount.setValue(0.5);

    assignMin.setSliderStyle(juce::Slider::LinearHorizontal);
    assignMin.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    assignMax.setSliderStyle(juce::Slider::LinearHorizontal);
    assignMax.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);

    presetLabel.setText("Presets", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour::fromFloatRGBA(0.9f, 0.9f, 1.0f, 1.0f));
    presetLabel.setFont(juce::Font(14.0f, juce::Font::bold));

    presetName.setTextToShowWhenEmpty("Preset name…", juce::Colours::grey);
    presetName.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    presetName.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromFloatRGBA(0.2f, 0.2f, 0.3f, 1.0f));
    presetName.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromFloatRGBA(0.8f, 0.8f, 0.9f, 1.0f));
    presetName.onReturnKey = [this]
    {
        saveCurrentAsName(presetName.getText().trim());
    };

    presetList.setJustificationType(juce::Justification::centredLeft);
    presetList.onChange = [this]
    {
        loadPresetByName(presetList.getText());
    };

    presetRefresh.onClick = [this] { refreshPresetUi(); };
    presetSave.onClick = [this]
    {
        const auto selected = presetList.getText().trim();
        if (selected.isNotEmpty())
            saveCurrentAsName(selected);
        else
            saveCurrentAsName(presetName.getText().trim());
    };
    presetSaveAs.onClick = [this] { saveCurrentAsName(presetName.getText().trim()); };
    presetDelete.onClick = [this]
    {
        const auto selected = presetList.getText().trim();
        if (selected.isEmpty())
            return;
        processor.getPresetManager().deletePreset(selected);
        editor.refreshPresetsFromDisk();
        refreshPresetUi();
    };

    moduleLabel.setText("FX Chain Order", juce::dontSendNotification);
    moduleLabel.setColour(juce::Label::textColourId, juce::Colour::fromFloatRGBA(0.9f, 0.9f, 1.0f, 1.0f));
    moduleLabel.setFont(juce::Font(14.0f, juce::Font::bold));

    modLabel.setText("Modulation Mapping", juce::dontSendNotification);
    modLabel.setColour(juce::Label::textColourId, juce::Colour::fromFloatRGBA(0.9f, 0.9f, 1.0f, 1.0f));
    modLabel.setFont(juce::Font(14.0f, juce::Font::bold));

    auto ids = processor.getParameterIDs();
    for (int i = 0; i < ids.size(); ++i)
    {
        if (ids[i] == "amount")
            continue;
        assignParam.addItem(ids[i], assignParam.getNumItems() + 1);
    }
    if (assignParam.getNumItems() > 0)
        assignParam.setSelectedItemIndex(0);

    assignParam.onChange = [this]
    {
        auto id = assignParam.getText();
        if (auto* param = processor.getAPVTS().getParameter(id))
        {
            auto range = param->getNormalisableRange();
            assignMin.setRange(range.start, range.end, 0.001);
            assignMax.setRange(range.start, range.end, 0.001);
            assignMin.setValue(range.start);
            assignMax.setValue(range.end);
        }
    };

    addAssign.onClick = [this]
    {
        ModMatrix::Assignment a;
        a.paramID = assignParam.getText();
        a.amount = (float) assignAmount.getValue();
        a.useRange = assignUseRange.getToggleState();
        a.min = (float) assignMin.getValue();
        a.max = (float) assignMax.getValue();
        if (a.paramID.isNotEmpty())
        {
            processor.getModMatrix().addAssignment(a);
            assignList.updateContent();
        }
    };

    removeAssign.onClick = [this]
    {
        int row = assignList.getSelectedRow();
        if (row >= 0)
        {
            processor.getModMatrix().removeAssignment(row);
            assignList.updateContent();
        }
    };
    rebuildParameterUI();
    refreshPresetUi();

    moveUp.onClick = [this]
    {
        int row = moduleList.getSelectedRow();
        if (row > 0)
        {
            processor.getFxChain().moveModule(row, row - 1);
            moduleList.updateContent();
            moduleList.selectRow(row - 1);
        }
    };

    moveDown.onClick = [this]
    {
        int row = moduleList.getSelectedRow();
        if (row >= 0 && row < processor.getFxChain().getModuleOrder().size() - 1)
        {
            processor.getFxChain().moveModule(row, row + 1);
            moduleList.updateContent();
            moduleList.selectRow(row + 1);
        }
    };
}

void TheRocketAudioProcessorEditor::DeveloperPanel::refreshPresetUi()
{
    const auto keepName = presetList.getText().trim();

    presetList.clear(juce::dontSendNotification);
    const auto names = processor.getPresetManager().getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetList.addItem(names[i], i + 1);

    if (names.isEmpty())
    {
        presetName.setText({}, juce::dontSendNotification);
        return;
    }

    const auto target = keepName.isNotEmpty() ? keepName : names[0];
    presetList.setText(target, juce::dontSendNotification);
    presetName.setText(target, juce::dontSendNotification);
}

void TheRocketAudioProcessorEditor::DeveloperPanel::loadPresetByName(const juce::String& name)
{
    const auto n = name.trim();
    if (n.isEmpty())
        return;
    editor.loadPresetByName(n);
    presetName.setText(n, juce::dontSendNotification);
}

void TheRocketAudioProcessorEditor::DeveloperPanel::saveCurrentAsName(const juce::String& name)
{
    const auto n = name.trim();
    if (n.isEmpty())
        return;

    processor.getPresetManager().savePreset(n);
    editor.refreshPresetsFromDisk();
    refreshPresetUi();
    loadPresetByName(n);
}

void TheRocketAudioProcessorEditor::DeveloperPanel::rebuildParameterUI()
{
    sliders.clear();
    toggles.clear();
    sliderAttachments.clear();
    buttonAttachments.clear();
    paramContent.removeAllChildren();

    auto& apvts = processor.getAPVTS();
    int y = 0;

    // Add section header
    auto* headerLabel = new juce::Label();
    headerLabel->setText("FX Parameters", juce::dontSendNotification);
    headerLabel->setColour(juce::Label::textColourId, juce::Colour::fromFloatRGBA(0.9f, 0.9f, 1.0f, 1.0f));
    headerLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    paramContent.addAndMakeVisible(headerLabel);
    headerLabel->setBounds(0, y, 450, 25);
    y += 30;

    auto ids = processor.getParameterIDs();
    for (int i = 0; i < ids.size(); ++i)
    {
        auto id = ids[i];
        if (id == "amount")
            continue;

        auto* param = apvts.getParameter(id);
        if (param == nullptr)
            continue;

        // Add parameter label
        auto* paramLabel = new juce::Label();
        paramLabel->setText(param->getName(64), juce::dontSendNotification);
        paramLabel->setColour(juce::Label::textColourId, juce::Colour::fromFloatRGBA(0.8f, 0.8f, 0.9f, 1.0f));
        paramLabel->setFont(juce::Font(12.0f));
        paramContent.addAndMakeVisible(paramLabel);
        paramLabel->setBounds(0, y, 200, 20);
        y += 25;

        if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
        {
            auto* t = new juce::ToggleButton(boolParam->name);
            t->setColour(juce::ToggleButton::tickColourId, juce::Colour::fromFloatRGBA(1.0f, 0.6f, 0.2f, 1.0f));
            t->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromFloatRGBA(0.5f, 0.5f, 0.5f, 1.0f));
            t->setColour(juce::ToggleButton::textColourId, juce::Colour::fromFloatRGBA(0.9f, 0.9f, 1.0f, 1.0f));
            
            toggles.add(t);
            paramContent.addAndMakeVisible(t);
            buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(apvts, id, *t));
            t->setBounds(220, y - 5, 200, 24);
            y += 35;
        }
        else
        {
            auto* s = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            s->setColour(juce::Slider::backgroundColourId, juce::Colour::fromFloatRGBA(0.2f, 0.2f, 0.3f, 1.0f));
            s->setColour(juce::Slider::trackColourId, juce::Colour::fromFloatRGBA(0.4f, 0.4f, 0.5f, 1.0f));
            s->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromFloatRGBA(1.0f, 0.6f, 0.2f, 1.0f));
            s->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromFloatRGBA(0.8f, 0.8f, 0.9f, 1.0f));
            s->setColour(juce::Slider::thumbColourId, juce::Colour::fromFloatRGBA(0.9f, 0.9f, 1.0f, 1.0f));
            s->setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromFloatRGBA(0.9f, 0.9f, 1.0f, 1.0f));
            s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromFloatRGBA(0.3f, 0.3f, 0.4f, 1.0f));
            s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromFloatRGBA(0.6f, 0.6f, 0.7f, 1.0f));
            
            s->setTextValueSuffix(" ");
            s->setName(param->getName(64));
            sliders.add(s);
            paramContent.addAndMakeVisible(s);
            sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, id, *s));

            s->setBounds(220, y - 5, 420, 32);
            y += 40;
        }
    }

    paramContent.setSize(450, y + 20);
}

void TheRocketAudioProcessorEditor::DeveloperPanel::rebuildModuleList()
{
    moduleList.updateContent();
}

void TheRocketAudioProcessorEditor::DeveloperPanel::resized()
{
    auto area = getLocalBounds().reduced(kPadding);
    
    // Create a more organized layout with sections
    auto leftPanel = area.removeFromLeft(240);
    auto rightPanel = area;

    // Preset tools
    auto presetSection = leftPanel.removeFromTop(120);
    auto presetHeader = presetSection.removeFromTop(25);
    presetLabel.setBounds(presetHeader);

    presetList.setBounds(presetSection.removeFromTop(24));
    presetName.setBounds(presetSection.removeFromTop(24).reduced(0, 2));
    auto presetBtns = presetSection.removeFromTop(24);
    presetRefresh.setBounds(presetBtns.removeFromLeft(75).reduced(2));
    presetSave.setBounds(presetBtns.removeFromLeft(55).reduced(2));
    presetSaveAs.setBounds(presetBtns.removeFromLeft(65).reduced(2));
    presetDelete.setBounds(presetBtns.removeFromLeft(60).reduced(2));
    
    // Module ordering section
    auto moduleSection = leftPanel.removeFromTop(juce::jmax(140, leftPanel.getHeight() - 200));
    auto moduleHeader = moduleSection.removeFromTop(25);
    moduleLabel.setBounds(moduleHeader);
    
    moduleList.setBounds(moduleSection);
    
    // Module control buttons
    auto btnArea = leftPanel.removeFromTop(40);
    moveUp.setBounds(btnArea.removeFromLeft(90).reduced(2));
    moveDown.setBounds(btnArea.removeFromLeft(90).reduced(2));
    
    // Modulation mapping section
    auto modSection = leftPanel;
    auto modHeader = modSection.removeFromTop(25);
    modLabel.setBounds(modHeader);
    
    auto row1 = modSection.removeFromTop(24);
    assignParam.setBounds(row1.removeFromLeft(140));
    assignAmount.setBounds(row1.removeFromLeft(120));
    addAssign.setBounds(row1.removeFromLeft(70));

    auto row2 = modSection.removeFromTop(24);
    assignUseRange.setBounds(row2.removeFromLeft(70));
    assignMin.setBounds(row2.removeFromLeft(100));
    assignMax.setBounds(row2.removeFromLeft(100));
    removeAssign.setBounds(row2.removeFromLeft(70));

    assignList.setBounds(modSection);

    // Parameter controls section
    paramViewport.setBounds(rightPanel);
    
    // Style buttons
    moveUp.setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.3f, 0.3f, 0.4f, 1.0f));
    moveUp.setColour(juce::TextButton::textColourOnId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    moveUp.setColour(juce::TextButton::textColourOffId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    
    moveDown.setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.3f, 0.3f, 0.4f, 1.0f));
    moveDown.setColour(juce::TextButton::textColourOnId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    moveDown.setColour(juce::TextButton::textColourOffId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    
    addAssign.setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.4f, 0.6f, 0.8f, 1.0f));
    addAssign.setColour(juce::TextButton::textColourOnId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    addAssign.setColour(juce::TextButton::textColourOffId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    
    removeAssign.setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.8f, 0.4f, 0.4f, 1.0f));
    removeAssign.setColour(juce::TextButton::textColourOnId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    removeAssign.setColour(juce::TextButton::textColourOffId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
}

TheRocketAudioProcessorEditor::TheRocketAudioProcessorEditor (TheRocketAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    DBG("TheRocketAudioProcessorEditor constructor called");
    backgroundImg = juce::ImageCache::getFromMemory(BinaryData::ui_background_png, BinaryData::ui_background_pngSize);
    cloudsImg = juce::ImageCache::getFromMemory(BinaryData::ui_clouds_png, BinaryData::ui_clouds_pngSize);
    panelImg = juce::ImageCache::getFromMemory(BinaryData::ui_panel_png, BinaryData::ui_panel_pngSize);
    rocketImg = juce::ImageCache::getFromMemory(BinaryData::ui_rocket_png, BinaryData::ui_rocket_pngSize);
    flameImg = juce::ImageCache::getFromMemory(BinaryData::ui_flame_png, BinaryData::ui_flame_pngSize);

    setLookAndFeel(&rocketLnf);

    amountKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    amountKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    amountKnob.setRange(0.0, 1.0, 0.001);
    amountKnob.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    
    addAndMakeVisible(amountKnob);

    amountAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(processor.getAPVTS(), "amount", amountKnob));

    addAndMakeVisible(presetBox);
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);
    addAndMakeVisible(devToggle);

    // Make preset controls visible with rocket theme
    presetBox.setAlpha(1.0f);
    prevButton.setAlpha(1.0f);
    nextButton.setAlpha(1.0f);
    devToggle.setAlpha(1.0f);

    presetBox.setJustificationType(juce::Justification::centredLeft);
    prevButton.setButtonText("<");
    nextButton.setButtonText(">");
    
    // Style preset controls
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromFloatRGBA(0.2f, 0.2f, 0.3f, 1.0f));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromFloatRGBA(0.8f, 0.8f, 0.9f, 1.0f));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    presetBox.setColour(juce::ComboBox::arrowColourId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    
    prevButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.3f, 0.3f, 0.4f, 1.0f));
    prevButton.setColour(juce::TextButton::textColourOnId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    prevButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    
    nextButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.3f, 0.3f, 0.4f, 1.0f));
    nextButton.setColour(juce::TextButton::textColourOnId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    nextButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    
    devToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    devToggle.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromFloatRGBA(0.5f, 0.5f, 0.5f, 1.0f));
    devToggle.setColour(juce::ToggleButton::textColourId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    devToggle.setColour(juce::ToggleButton::textColourId, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));

    refreshPresetList();
    if (presetBox.getNumItems() > 0)
        loadPresetFromIndex(0);

    prevButton.onClick = [this]
    {
        int idx = presetBox.getSelectedItemIndex();
        if (idx > 0)
            loadPresetFromIndex(idx - 1);
    };

    nextButton.onClick = [this]
    {
        int idx = presetBox.getSelectedItemIndex();
        if (idx < presetBox.getNumItems() - 1)
            loadPresetFromIndex(idx + 1);
    };

    presetBox.onChange = [this]
    {
        loadPresetFromIndex(presetBox.getSelectedItemIndex());
    };

    devToggle.setToggleState(false, juce::dontSendNotification);
#if defined(ROCKET_INTERNAL_UI) && ROCKET_INTERNAL_UI
    devToggle.setVisible(true);
#else
    devToggle.setVisible(false); // Hide internal UI for public builds
#endif
    devToggle.onClick = [this]
    {
        const bool show = devToggle.getToggleState();
        if (show && devPanel == nullptr)
        {
            devPanel.reset(new DeveloperPanel(*this));
            addAndMakeVisible(devPanel.get());
        }
        if (devPanel)
            devPanel->setVisible(show);
        resized();
    };

    if (backgroundImg.isValid())
    {
        DBG("Background image valid, setting size to " + juce::String(backgroundImg.getWidth()) + " x " + juce::String(backgroundImg.getHeight()));
        // Use portrait orientation - wider height for rocket animation
        setSize(600, 900);
    }
    else
    {
        DBG("Using default portrait size 600x900");
        setSize(600, 900);
    }

    // Resizable portrait UI with fixed aspect ratio
    boundsConstrainer.setFixedAspectRatio(600.0 / 900.0);
    setResizable(true, true);
    setResizeLimits(360, 540, 960, 1440);
    setConstrainer(&boundsConstrainer);

    resizer.reset(new juce::ResizableCornerComponent(this, &boundsConstrainer));
    addAndMakeVisible(resizer.get());
    
    setVisible(true);
    setOpaque(true);
    toFront(true);

    uiAmountSmoothed.reset(60.0, 0.12);
    uiAmountSmoothed.setCurrentAndTargetValue(processor.getAPVTS().getRawParameterValue("amount")->load());
    
    // Start a timer to continuously repaint for animation
    startTimer(16); // ~60 FPS
    
    DBG("TheRocketAudioProcessorEditor constructor finished, size: " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
}

TheRocketAudioProcessorEditor::~TheRocketAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void TheRocketAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.fillAll(juce::Colours::black);
    
    // Draw background without aspect distortion (cropped if needed)
    if (backgroundImg.isValid())
    {
        g.drawImageWithin(backgroundImg, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::fillDestination);
    }

    // Get the current knob value (0.0 to 1.0)
    const float knobValue = juce::jlimit(0.0f, 1.0f, uiAmountSmoothed.getCurrentValue());

    const auto layout = makeMainLayout(getLocalBounds(), panelImg);

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(layout.animationArea);

        // Draw clouds - keep correct aspect ratio
        if (cloudsImg.isValid())
        {
            const float cloudAspect = (float) cloudsImg.getWidth() / (float) cloudsImg.getHeight();
            const int cloudW = juce::roundToInt(190.0f * layout.scale);
            const int cloudH = juce::roundToInt(cloudW / cloudAspect);

            const float topBand = (float) juce::roundToInt(28.0f * layout.scale);

            const float c1x = 22.0f * layout.scale + knobValue * 18.0f * layout.scale;
            const float c1y = topBand + 40.0f * layout.scale - knobValue * 60.0f * layout.scale;
            g.setOpacity(0.85f);
            g.drawImage(cloudsImg, juce::roundToInt(c1x), juce::roundToInt(c1y), cloudW, cloudH, 0, 0, cloudsImg.getWidth(),
                        cloudsImg.getHeight());

            const float c2x = (float) (getWidth() / 2 - cloudW / 2) + knobValue * 25.0f * layout.scale;
            const float c2y = topBand + 12.0f * layout.scale - knobValue * 72.0f * layout.scale;
            g.setOpacity(0.80f);
            g.drawImage(cloudsImg, juce::roundToInt(c2x), juce::roundToInt(c2y), cloudW, cloudH, 0, 0, cloudsImg.getWidth(),
                        cloudsImg.getHeight());

            const float c3x = (float) (getWidth() - cloudW) - 22.0f * layout.scale - knobValue * 20.0f * layout.scale;
            const float c3y = topBand + 68.0f * layout.scale - knobValue * 56.0f * layout.scale;
            g.setOpacity(0.85f);
            g.drawImage(cloudsImg, juce::roundToInt(c3x), juce::roundToInt(c3y), cloudW, cloudH, 0, 0, cloudsImg.getWidth(),
                        cloudsImg.getHeight());
        }

        const float topMargin = 40.0f * layout.scale;
        const float padOffset = 18.0f * layout.scale;

        float rocketY = 0.0f;
        int rocketW = 0;
        int rocketH = 0;

        // Draw rocket - launches upward from above the panel
        if (rocketImg.isValid())
        {
            // Crop transparent padding from the source art for consistent sizing.
            const juce::Rectangle<int> rocketSrc { 36, 47, 54, 206 };
            const float rocketAspect = (float) rocketSrc.getWidth() / (float) rocketSrc.getHeight();
            rocketH = juce::roundToInt(120.0f * layout.scale);
            rocketW = juce::roundToInt((float) rocketH * rocketAspect);

            const float maxLift = juce::jmax(0.0f, (float) layout.animationArea.getHeight() - topMargin - (float) rocketH - padOffset);
            const float baseY = (float) layout.animationArea.getHeight() - padOffset - (float) rocketH;
            rocketY = baseY - knobValue * knobValue * maxLift;

            g.setOpacity(1.0f);
            g.drawImage(rocketImg, getWidth() / 2 - rocketW / 2, juce::roundToInt(rocketY), rocketW, rocketH, rocketSrc.getX(),
                        rocketSrc.getY(), rocketSrc.getWidth(), rocketSrc.getHeight());
        }

        // Draw animated flame - anchored under the rocket (stays clipped above the panel)
        if (flameImg.isValid() && rocketImg.isValid() && knobValue > 0.03f && rocketH > 0)
        {
            const juce::Rectangle<int> flameSrc { 7, 231, 106, 314 };
            const float flameAspect = (float) flameSrc.getWidth() / (float) flameSrc.getHeight();
            const float intensity = juce::jlimit(0.0f, 1.0f, (knobValue - 0.03f) / 0.97f);

            const float flameScale = 0.65f + intensity * 0.95f;
            const int flameH = juce::roundToInt((float) rocketH * 1.15f * flameScale);
            const int flameW = juce::roundToInt((float) flameH * flameAspect);

            const float overlap = (float) rocketH * 0.10f;
            const float flameY = rocketY + (float) rocketH - overlap;

            g.setOpacity(0.65f + intensity * 0.35f);
            g.drawImage(flameImg, getWidth() / 2 - flameW / 2, juce::roundToInt(flameY), flameW, flameH, flameSrc.getX(),
                        flameSrc.getY(), flameSrc.getWidth(), flameSrc.getHeight());
        }
    }

    // Darken panel area subtly (helps controls read on bright backgrounds)
    {
        const auto fadeTop = juce::jmax(0, layout.panel.getY() - juce::roundToInt(80.0f * layout.scale));
        juce::ColourGradient grad(juce::Colours::transparentBlack, 0.0f, (float) fadeTop,
                                  juce::Colours::black.withAlpha(0.72f), 0.0f, (float) layout.panel.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRect(0, fadeTop, getWidth(), getHeight() - fadeTop);
    }

    if (panelImg.isValid())
    {
        g.setOpacity(1.0f);
        g.drawImage(panelImg, layout.panel.getX(), layout.panel.getY(), layout.panel.getWidth(), layout.panel.getHeight(), 0, 0,
                    panelImg.getWidth(), panelImg.getHeight());
    }
}

void TheRocketAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    if (resizer)
    {
        resizer->setBounds(getLocalBounds().removeFromBottom(18).removeFromRight(18));
        resizer->toFront(false);
    }
    
    if (panelImg.isValid())
    {
        const auto layout = makeMainLayout(getLocalBounds(), panelImg);

        amountKnob.setBounds(layout.knob);
        prevButton.setBounds(layout.prev);
        nextButton.setBounds(layout.next);
        presetBox.setBounds(layout.preset);
        devToggle.setBounds(layout.devToggle);
    }
    else
    {
        auto header = area.removeFromTop(50);
        presetBox.setBounds(header.removeFromLeft(200).reduced(2));
        prevButton.setBounds(header.removeFromLeft(35).reduced(2));
        nextButton.setBounds(header.removeFromLeft(35).reduced(2));
        devToggle.setBounds(header.removeFromLeft(80).reduced(2));
        amountKnob.setBounds(area.removeFromBottom(150).withSizeKeepingCentre(120, 120));
    }

    if (devPanel && devPanel->isVisible())
    {
        devPanel->setBounds(area);
        amountKnob.setVisible(false);
        return;
    }

    amountKnob.setVisible(true);
}

void TheRocketAudioProcessorEditor::refreshPresetList()
{
    try {
        const auto keepName = presetBox.getText().trim();
        presetBox.clear();
        auto names = processor.getPresetManager().getPresetNames();
        for (int i = 0; i < names.size(); ++i)
            presetBox.addItem(names[i], i + 1);

        if (names.size() > 0)
        {
            int idx = names.indexOf(keepName);
            if (!juce::isPositiveAndBelow(idx, names.size()))
                idx = 0;
            presetBox.setSelectedItemIndex(idx, juce::dontSendNotification);
        }
    } catch (const std::exception& e) {
        juce::ignoreUnused(e);
        DBG("Exception in refreshPresetList: " << e.what());
    } catch (...) {
        DBG("Unknown exception in refreshPresetList");
    }
}

void TheRocketAudioProcessorEditor::refreshPresetsFromDisk()
{
    refreshPresetList();
}

void TheRocketAudioProcessorEditor::loadPresetByName(const juce::String& name)
{
    const auto n = name.trim();
    if (n.isEmpty())
        return;

    processor.getPresetManager().loadPreset(n);
    processor.notifyPresetLoaded();

    refreshPresetList();

    auto names = processor.getPresetManager().getPresetNames();
    const int idx = names.indexOf(n);
    if (juce::isPositiveAndBelow(idx, names.size()))
        presetBox.setSelectedItemIndex(idx, juce::dontSendNotification);
}

void TheRocketAudioProcessorEditor::loadPresetFromIndex(int index)
{
    auto names = processor.getPresetManager().getPresetNames();
    if (!juce::isPositiveAndBelow(index, names.size()))
        return;

    processor.getPresetManager().loadPreset(names[index]);
    processor.notifyPresetLoaded();
    presetBox.setSelectedItemIndex(index, juce::dontSendNotification);
}

void TheRocketAudioProcessorEditor::timerCallback()
{
    const float amountTarget = processor.getAPVTS().getRawParameterValue("amount")->load();
    uiAmountSmoothed.setTargetValue(amountTarget);
    uiAmountSmoothed.getNextValue();
    repaint();
}
