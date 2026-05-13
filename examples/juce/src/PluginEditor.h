#pragma once

#include "PluginProcessor.h"

/// Custom LookAndFeel for Jucerguy
class JucerguyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JucerguyLookAndFeel()
    {
        // Dark theme colors
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff2b2b2b));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff6c9ced));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff4a4a4a));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff6c9ced));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff4a4a4a));
        setColour(juce::Label::textColourId, juce::Colours::white);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Background track
        g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
        g.fillEllipse(rx, ry, rw, rw);

        // Inner circle
        g.setColour(juce::Colour(0xff1a1a2e));
        g.fillEllipse(rx + 4, ry + 4, rw - 8, rw - 8);

        // Value arc
        juce::Path arcPath;
        arcPath.addCentredArc(centreX, centreY, radius - 2, radius - 2,
                              0.0f, rotaryStartAngle, angle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(arcPath, juce::PathStrokeType(4.0f));

        // Pointer
        juce::Path pointer;
        auto pointerLength = radius * 0.6f;
        auto pointerThickness = 3.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -radius + 6, pointerThickness, pointerLength, 1.5f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colours::white);
        g.fillPath(pointer);
    }
};

/// Clickable status pill in the editor's top-right corner. Shows a
/// colored dot plus a short status label ("Licensed", "Expired", etc.).
/// Click opens the license modal.
class LicenseStatusPill : public juce::Component, public juce::Timer
{
public:
    std::function<void()> onClicked;

    LicenseStatusPill();
    void paint(juce::Graphics&) override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    int currentStatusInt = -1;
};

/// Sticky banner across the top of the editor for bad license states
/// (NotActivated / Expired / Tampered). Hidden otherwise. Click opens
/// the license modal.
class LicenseBanner : public juce::Component, public juce::Timer
{
public:
    std::function<void()> onClicked;

    LicenseBanner();
    void paint(juce::Graphics&) override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    int currentStatusInt = -1;
};

/// License modal overlay: status pill, key input, Activate / Deactivate,
/// error line. SDK callbacks run on a worker thread; we marshal back to
/// the message thread via SafePointer + MessageManager::callAsync.
class LicenseModalComponent : public juce::Component, public juce::Timer
{
public:
    LicenseModalComponent();
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;
    void visibilityChanged() override;

private:
    void refreshDisplay();
    void onActivate();
    void onDeactivate();

    juce::Label statusPill;
    juce::TextEditor keyInput;
    juce::TextButton activateBtn;
    juce::TextButton deactivateBtn;
    juce::TextButton closeBtn;
    juce::Label errorLabel;

    std::atomic<bool> busy { false };
    juce::String pendingError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseModalComponent)
};

/// Plugin editor for Jucerguy
class JucerguyEditor : public juce::AudioProcessorEditor
{
public:
    explicit JucerguyEditor(JucerguyProcessor&);
    ~JucerguyEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    JucerguyProcessor& processorRef;

    // LookAndFeel (declare BEFORE components that use it!)
    JucerguyLookAndFeel lookAndFeel;

    // UI components
    juce::Slider gainSlider;
    juce::Label gainLabel;

    // License UI
    LicenseStatusPill licensePill;
    LicenseBanner licenseBanner;
    LicenseModalComponent licenseModal;

    // Attachments (declare AFTER components!)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JucerguyEditor)
};
