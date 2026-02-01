#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

class StandaloneWindow : public juce::DocumentWindow
{
public:
    StandaloneWindow()
        : DocumentWindow("The Rocket",
                        juce::Desktop::getInstance().getDefaultLookAndFeel()
                            .findColour(ResizableWindow::backgroundColourId),
                        DocumentWindow::allButtons),
          processor(std::make_unique<TheRocketAudioProcessor>())
    {
        setUsingNativeTitleBar(true);

        auto* editor = processor->createEditor();

        // Keep portrait aspect ratio and fit inside the current display's usable area (taskbar-safe).
        constrainer.setFixedAspectRatio(600.0 / 900.0);
        setConstrainer(&constrainer);

        if (editor != nullptr)
        {
            setContentOwned(editor, true);
        }
        
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto userArea = display != nullptr ? display->userArea.reduced(20) : juce::Rectangle<int>(0, 0, 900, 900);

        const double aspect = 600.0 / 900.0;
        int targetH = juce::jmin(900, userArea.getHeight());
        int targetW = juce::roundToInt(targetH * aspect);
        if (targetW > userArea.getWidth())
        {
            targetW = userArea.getWidth();
            targetH = juce::roundToInt(targetW / aspect);
        }

        setBounds(userArea.withSizeKeepingCentre(targetW, targetH));
        setVisible(true);
        setResizable(true, true);
        setResizeLimits(360, 540, 960, 1440);
    }

    void closeButtonPressed() override
    {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    std::unique_ptr<TheRocketAudioProcessor> processor;
    juce::ComponentBoundsConstrainer constrainer;
};

class StandaloneApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "The Rocket"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        window = std::make_unique<StandaloneWindow>();
    }

    void shutdown() override
    {
        window = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

    void suspended() override {}
    void resumed() override {}
    void unhandledException(const std::exception*, const juce::String&, int) override {}

private:
    std::unique_ptr<StandaloneWindow> window;
};

START_JUCE_APPLICATION(StandaloneApp)
