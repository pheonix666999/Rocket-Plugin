#pragma once

#include <JuceHeader.h>

class RocketLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RocketLookAndFeel()
    {
        const auto panelBg = juce::Colour::fromRGB(40, 42, 55);
        const auto panelBg2 = juce::Colour::fromRGB(55, 58, 74);
        const auto outline = juce::Colour::fromRGB(200, 205, 220);
        const auto text = juce::Colour::fromRGB(245, 246, 250);
        const auto accent = juce::Colour::fromRGB(214, 78, 92);   // warm red
        const auto accent2 = juce::Colour::fromRGB(64, 192, 220); // cyan

        setColour(juce::Slider::rotarySliderFillColourId, accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, outline.withAlpha(0.45f));
        setColour(juce::Slider::thumbColourId, accent2);

        setColour(juce::ComboBox::backgroundColourId, panelBg.withAlpha(0.92f));
        setColour(juce::ComboBox::outlineColourId, outline.withAlpha(0.7f));
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::arrowColourId, text.withAlpha(0.9f));

        setColour(juce::PopupMenu::backgroundColourId, panelBg2.withAlpha(0.98f));
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.35f));
        setColour(juce::PopupMenu::highlightedTextColourId, text);

        setColour(juce::TextButton::buttonColourId, panelBg.withAlpha(0.9f));
        setColour(juce::TextButton::buttonOnColourId, panelBg2.withAlpha(0.95f));
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::TextButton::textColourOnId, text);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height)
                                .reduced(juce::jmax(2.0f, 0.06f * (float) juce::jmin(width, height)));

        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const auto outline = findColour(juce::Slider::rotarySliderOutlineColourId);
        const auto fill = findColour(juce::Slider::rotarySliderFillColourId);
        const auto thumb = findColour(juce::Slider::thumbColourId);

        const float ringThickness = juce::jlimit(3.0f, 10.0f, radius * 0.13f);

        juce::Path bgArc;
        bgArc.addCentredArc(centre.x, centre.y, radius - ringThickness, radius - ringThickness, 0.0f, rotaryStartAngle,
                            rotaryEndAngle, true);
        g.setColour(outline);
        g.strokePath(bgArc, juce::PathStrokeType(ringThickness, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, radius - ringThickness, radius - ringThickness, 0.0f, rotaryStartAngle,
                               angle, true);
        g.setColour(fill);
        g.strokePath(valueArc, juce::PathStrokeType(ringThickness, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        const auto inner = bounds.reduced(ringThickness * 1.15f);
        g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(22, 22, 28), inner.getTopLeft(),
                                              juce::Colour::fromRGB(44, 46, 58), inner.getBottomRight(), false));
        g.fillEllipse(inner);

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawEllipse(inner, 1.0f);

        const float dotRadius = juce::jlimit(3.0f, 7.5f, radius * 0.10f);
        const float dotDistance = radius - ringThickness * 0.65f;
        const juce::Point<float> dot { centre.x + dotDistance * std::cos(angle),
                                       centre.y + dotDistance * std::sin(angle) };

        g.setColour(thumb);
        g.fillEllipse(dot.x - dotRadius, dot.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawEllipse(dot.x - dotRadius, dot.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f, 1.0f);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        auto base = findColour(button.getToggleState() ? juce::TextButton::buttonOnColourId
                                                       : juce::TextButton::buttonColourId);
        if (isButtonDown)
            base = base.brighter(0.15f);
        else if (isMouseOverButton)
            base = base.brighter(0.08f);

        g.setColour(base);
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool,
                      int,
                      int,
                      int,
                      int,
                      juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);

        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, 7.0f);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, 7.0f, 1.0f);

        const auto arrow = box.findColour(juce::ComboBox::arrowColourId);
        g.setColour(arrow);

        const float arrowZoneW = juce::jmax(20.0f, bounds.getHeight());
        const auto arrowZone = juce::Rectangle<float>(bounds.getRight() - arrowZoneW, bounds.getY(), arrowZoneW, bounds.getHeight())
                                   .reduced(bounds.getHeight() * 0.22f);

        juce::Path p;
        const auto cx = arrowZone.getCentreX();
        const auto cy = arrowZone.getCentreY();
        const auto w = arrowZone.getWidth() * 0.30f;
        const auto h = arrowZone.getHeight() * 0.22f;
        p.startNewSubPath(cx - w, cy - h);
        p.lineTo(cx + w, cy - h);
        p.lineTo(cx, cy + h);
        p.closeSubPath();
        g.fillPath(p);
    }
};
