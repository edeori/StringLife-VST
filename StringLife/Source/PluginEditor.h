#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

#include "PluginProcessor.h"
#include "StringLifeLookAndFeel.h"

// Simple vertical peak-hold level meter, -60dB to 0dB.
class LevelMeter  : public juce::Component,
                     private juce::Timer
{
public:
    explicit LevelMeter (std::function<float()> levelDbProviderIn);
    ~LevelMeter() override;

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    std::function<float()> levelDbProvider;
    float displayedLevelDb = -100.0f;
    float peakHoldDb = -100.0f;
    int peakHoldCounter = 0;

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

#if STRINGLIFE_ENABLE_LICENSING
class LicenseOverlayComponent : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
};
#endif

class StringLifeAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit StringLifeAudioProcessorEditor (StringLifeAudioProcessor&);
    ~StringLifeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void setChoiceParam (const juce::String& paramID, int choiceIndex);
    void updateFonts (float scale);
    void updateInstrumentButtons();
    void updateMonitorButtons();
    void updateCompareButtonLabel();
    void updateAdvancedLabel (juce::Label& valueLabel, juce::Slider& slider);

#if STRINGLIFE_ENABLE_LICENSING
    void updateLicenseUi();
    void updateLicenseOverlayVisibility();
    void showActivationDialog();
#endif

    void drawPanelFrame (juce::Graphics& g);
    void drawLogo (juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawMeterScale (juce::Graphics& g);

    static juce::String withLetterSpacing (const juce::String& text);

    StringLifeLookAndFeel lookAndFeel;
    juce::Image backgroundImage;
    juce::Image logoImage;

    juce::AudioProcessorValueTreeState& apvts;

    juce::Label manufacturerLabel;
    juce::Label titleLabel;

    juce::TextButton guitarButton { "GUITAR" };
    juce::TextButton bassButton   { "BASS" };

    juce::Slider stringAgeSlider;
    juce::Label deadLabel, freshLabel, stringAgeValueLabel;

    juce::Slider presenceSlider, attackSlider, harmonicSlider, sustainSlider;
    juce::Label presenceNameLabel, attackNameLabel, harmonicNameLabel, sustainNameLabel;
    juce::Label presenceValueLabel, attackValueLabel, harmonicValueLabel, sustainValueLabel;

    juce::Label monitorLabel;
    juce::TextButton normalButton { "NORMAL" };
    juce::TextButton deltaButton  { "DELTA" };

    // Independent of the Normal/Delta radio pair above: BYPASS is a hard,
    // host-visible on/off; A/B is a quick compare toggle whose label always
    // shows which side ("A" = processed, "B" = dry) you're currently hearing.
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton abButton     { "A" };

    juce::Slider wetDrySlider;
    juce::Label wetDryLabel;

    juce::Label metersLabel;
    LevelMeter inputMeter;
    LevelMeter outputMeter;

#if STRINGLIFE_ENABLE_LICENSING
    LicenseOverlayComponent licenseOverlay;
    juce::Label licenseOverlayTitleLabel;
    juce::Label licenseOverlayStatusLabel;
    juce::TextButton activateButton { "ACTIVATE" };
    juce::TextButton trialButton { "TRIAL" };
    bool licenseOverlayDismissed = false;
#endif

    juce::Label versionLabel;

    // Bounds cached by resized() so paint() can draw the logo and meter
    // scale where the components laid out around them actually are.
    juce::Rectangle<int> meterScaleBounds;
    juce::Rectangle<float> logoBounds;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> stringAgeAttachment, presenceAttachment, attackAttachment,
                                       harmonicAttachment, sustainAttachment, wetDryAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, abAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringLifeAudioProcessorEditor)
};
