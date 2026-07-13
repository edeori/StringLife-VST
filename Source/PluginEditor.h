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
    void updateAdvancedLabel (juce::Label& valueLabel, juce::Slider& slider);

#if STRINGLIFE_ENABLE_LICENSING
    void updateLicenseUi();
    void updateLicenseOverlayVisibility();
    void showActivationDialog();
#endif

    void drawPanelFrame (juce::Graphics& g);
    void drawLogo (juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawStringArt (juce::Graphics& g, juce::Rectangle<int> bounds);

    static juce::String withLetterSpacing (const juce::String& text);

    StringLifeLookAndFeel lookAndFeel;
    juce::Image backgroundImage;
    juce::Image logoImage;

    juce::AudioProcessorValueTreeState& apvts;

    juce::Label manufacturerLabel;
    juce::Label titleLabel;
    juce::Label taglineLabel;

    juce::TextButton guitarButton { "GUITAR" };
    juce::TextButton bassButton   { "BASS" };

    // String age panel: title/kicker header, decorative "strings" backdrop,
    // Revival readout, then the macro slider with its three range labels.
    juce::Label stringAgeTitleLabel, macroControlKicker;
    juce::Label revivalKicker, stringAgeValueLabel;
    juce::Slider stringAgeSlider;
    juce::Label deadLabel, balancedLabel, freshLabel;

    juce::Slider presenceSlider, attackSlider, harmonicSlider, sustainSlider;
    juce::Label presenceNameLabel, attackNameLabel, harmonicNameLabel, sustainNameLabel;
    juce::Label presenceValueLabel, attackValueLabel, harmonicValueLabel, sustainValueLabel;

    // Monitor Normal/Delta now lives top-right of the title row, not in a
    // separate bottom box - see the refreshed mockup.
    juce::Label monitorKicker;
    juce::TextButton normalButton { "NORMAL" };
    juce::TextButton deltaButton  { "DELTA" };

    // Independent of the Normal/Delta radio pair above: BYPASS is a hard,
    // host-visible on/off; A/B is a quick compare toggle whose label always
    // shows which side ("A" = processed, "B" = dry) you're currently hearing.
    // Both now live in the header, next to the version number.
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton abButton     { "A / B" };

    // Input/Output panel (right column of the main row): title, two vertical
    // meters with their own In/Out kickers, and the Mix knob + field.
    juce::Label ioTitleLabel;
    juce::Label inKicker, outKicker;
    LevelMeter inputMeter;
    LevelMeter outputMeter;

    juce::Slider wetDrySlider;
    juce::Label wetDryLabel, wetDryValueLabel;

#if STRINGLIFE_ENABLE_LICENSING
    LicenseOverlayComponent licenseOverlay;
    juce::Label licenseOverlayTitleLabel;
    juce::Label licenseOverlayStatusLabel;
    juce::TextButton activateButton { "ACTIVATE" };
    juce::TextButton trialButton { "TRIAL" };
    bool licenseOverlayDismissed = false;
#endif

    juce::Label versionLabel;

    // Bounds cached by resized() so paint() can draw the logo and the
    // decorative string art where the components laid out around them
    // actually are.
    juce::Rectangle<int> stringArtBounds;
    juce::Rectangle<float> logoBounds;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> stringAgeAttachment, presenceAttachment, attackAttachment,
                                       harmonicAttachment, sustainAttachment, wetDryAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, abAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringLifeAudioProcessorEditor)
};
