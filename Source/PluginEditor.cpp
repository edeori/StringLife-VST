#include "PluginEditor.h"
#include "StringLifeFonts.h"
#include "BinaryData.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
    constexpr float panelCornerSize = 10.0f;

    // Reference window size the design-space pixel values in resized() are
    // measured at - the window is resizable with this aspect ratio locked,
    // and every position, size and font scales uniformly from here. This
    // now matches Assets/background_panel.png (820x626) exactly, which is
    // itself a literal 1:1 render of the mockup's own CSS grid (see
    // Mockup/stringlife-ui.html) - so every design-space pixel value below
    // is copied straight from that stylesheet rather than measured/guessed.
    constexpr int designWidth  = 820;
    constexpr int designHeight = 626;
}

#if STRINGLIFE_ENABLE_LICENSING
void LicenseOverlayComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff020304).withAlpha (0.96f));
    g.fillRoundedRectangle (bounds, panelCornerSize);

    auto popup = bounds.withSizeKeepingCentre (bounds.getWidth() * 0.55f, bounds.getHeight() * 0.34f);
    g.setColour (juce::Colour (0xff090c10));
    g.fillRoundedRectangle (popup, 14.0f);

    auto shadow = popup.translated (0.0f, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (shadow, 14.0f, 5.0f);

    g.setColour (juce::Colours::white.withAlpha (0.26f));
    g.drawRoundedRectangle (popup, 14.0f, 1.4f);
}
#endif

//==============================================================================
LevelMeter::LevelMeter (std::function<float()> levelDbProviderIn)
    : levelDbProvider (std::move (levelDbProviderIn))
{
    startTimerHz (30);
}

LevelMeter::~LevelMeter()
{
    stopTimer();
}

void LevelMeter::timerCallback()
{
    displayedLevelDb = levelDbProvider ? levelDbProvider() : minDb;

    if (displayedLevelDb >= peakHoldDb)
    {
        peakHoldDb = displayedLevelDb;
        peakHoldCounter = 0;
    }
    else if (++peakHoldCounter > 30) // ~1 second hold at 30Hz before decaying
    {
        peakHoldDb = juce::jmax (minDb, peakHoldDb - 0.5f);
    }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    constexpr float cornerSize = 8.0f; // matches .vertical-meter's own border-radius

    g.setColour (juce::Colour (0xff040609));
    g.fillRoundedRectangle (bounds, cornerSize);

    auto normalise = [] (float db)
    {
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    };

    auto levelFraction = normalise (displayedLevelDb);
    auto meterBounds = bounds.reduced (2.0f).withTop (bounds.getBottom() - 2.0f - (bounds.getHeight() - 4.0f) * levelFraction);

    if (! meterBounds.isEmpty())
    {
        // Three-stop gradient matching .vertical-meter span in the mockup:
        // accent (low) -> yellow-green (mid) -> red (peak).
        juce::ColourGradient fillGradient (StringLifeLookAndFeel::getAccentColour(), 0.0f, bounds.getBottom(),
                                           juce::Colour (0xffff565b), 0.0f, bounds.getY(), false);
        fillGradient.addColour (0.75, juce::Colour (0xffd6e447));
        g.setGradientFill (fillGradient);
        g.fillRoundedRectangle (meterBounds, cornerSize * 0.6f);
    }

    auto peakY = bounds.getBottom() - bounds.getHeight() * normalise (peakHoldDb);
    g.setColour (juce::Colours::white);
    g.fillRect (bounds.getX(), peakY - 1.0f, bounds.getWidth(), 2.0f);

    g.setColour (juce::Colour (0xff2c333b));
    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);
}

//==============================================================================
juce::String StringLifeAudioProcessorEditor::withLetterSpacing (const juce::String& text)
{
    juce::String result;

    for (int i = 0; i < text.length(); ++i)
    {
        result << text[i];

        if (i < text.length() - 1)
            result << " ";
    }

    return result;
}

StringLifeAudioProcessorEditor::StringLifeAudioProcessorEditor (StringLifeAudioProcessor& p)
    : AudioProcessorEditor (&p),
      apvts (p.apvts),
      inputMeter  ([&p] { return p.getInputLevelDb(); }),
      outputMeter ([&p] { return p.getOutputLevelDb(); })
{
    setLookAndFeel (&lookAndFeel);

    backgroundImage = juce::ImageFileFormat::loadFrom (BinaryData::background_panel_png, (size_t) BinaryData::background_panel_pngSize);
    logoImage       = juce::ImageFileFormat::loadFrom (BinaryData::moth_logo_png, (size_t) BinaryData::moth_logo_pngSize);

    // Fonts for all labels are set (and re-set on every resize) by
    // updateFonts(), so they scale together with the layout.
    manufacturerLabel.setText (withLetterSpacing ("MOTH PRODUCTION"), juce::dontSendNotification);
    manufacturerLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (manufacturerLabel);

    titleLabel.setText ("StringLife", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.95f));
    addAndMakeVisible (titleLabel);

    taglineLabel.setText (withLetterSpacing ("RESTORE TIRED STRINGS"), juce::dontSendNotification);
    taglineLabel.setColour (juce::Label::textColourId, juce::Colours::grey.withAlpha (0.85f));
    addAndMakeVisible (taglineLabel);

    guitarButton.setClickingTogglesState (false);
    bassButton.setClickingTogglesState (false);
    StringLifeLookAndFeel::markAsInstrumentButton (guitarButton);
    StringLifeLookAndFeel::markAsInstrumentButton (bassButton);
    guitarButton.onClick = [this] { setChoiceParam (ParamIDs::instrument, 0); };
    bassButton.onClick   = [this] { setChoiceParam (ParamIDs::instrument, 1); };
    addAndMakeVisible (guitarButton);
    addAndMakeVisible (bassButton);

    monitorKicker.setText (withLetterSpacing ("MONITOR"), juce::dontSendNotification);
    monitorKicker.setJustificationType (juce::Justification::centredRight);
    monitorKicker.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (monitorKicker);

    // Normal/Delta is a simple two-way pill-tab selector, exactly the same
    // visual role as Guitar/Bass above - reuses that same button tier rather
    // than inventing a near-identical new one.
    normalButton.setClickingTogglesState (false);
    deltaButton.setClickingTogglesState (false);
    StringLifeLookAndFeel::markAsInstrumentButton (normalButton);
    StringLifeLookAndFeel::markAsInstrumentButton (deltaButton);
    normalButton.onClick = [this] { setChoiceParam (ParamIDs::monitorMode, 0); };
    deltaButton.onClick  = [this] { setChoiceParam (ParamIDs::monitorMode, 1); };
    addAndMakeVisible (normalButton);
    addAndMakeVisible (deltaButton);

    // Header actions: A/B compare and hard Bypass, top-right next to the
    // version number.
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::bypass, bypassButton);

    abButton.setClickingTogglesState (true);
    StringLifeLookAndFeel::markAsAlwaysActive (abButton);
    addAndMakeVisible (abButton);
    abAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::abCompare, abButton);

    stringAgeTitleLabel.setText (withLetterSpacing ("STRING AGE"), juce::dontSendNotification);
    stringAgeTitleLabel.setColour (juce::Label::textColourId, StringLifeLookAndFeel::getSectionTitleColour());
    addAndMakeVisible (stringAgeTitleLabel);

    macroControlKicker.setText (withLetterSpacing ("MACRO CONTROL"), juce::dontSendNotification);
    macroControlKicker.setJustificationType (juce::Justification::centredRight);
    macroControlKicker.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (macroControlKicker);

    revivalKicker.setText (withLetterSpacing ("REVIVAL"), juce::dontSendNotification);
    revivalKicker.setJustificationType (juce::Justification::centred);
    revivalKicker.setColour (juce::Label::textColourId, StringLifeLookAndFeel::getAccentColour());
    addAndMakeVisible (revivalKicker);

    stringAgeValueLabel.setJustificationType (juce::Justification::centred);
    stringAgeValueLabel.setColour (juce::Label::textColourId, StringLifeLookAndFeel::getAccentHotColour());
    addAndMakeVisible (stringAgeValueLabel);

    deadLabel.setText (withLetterSpacing ("DEAD"), juce::dontSendNotification);
    balancedLabel.setText (withLetterSpacing ("BALANCED"), juce::dontSendNotification);
    balancedLabel.setJustificationType (juce::Justification::centred);
    freshLabel.setText (withLetterSpacing ("FRESH"), juce::dontSendNotification);
    freshLabel.setJustificationType (juce::Justification::centredRight);
    for (auto* l : { &deadLabel, &balancedLabel, &freshLabel })
    {
        l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (l);
    }

    stringAgeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stringAgeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (stringAgeSlider);
    stringAgeAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::stringAge, stringAgeSlider);

    auto setupAdvancedSlider = [this] (juce::Slider& slider, juce::Label& nameLabel, juce::Label& valueLabel,
                                        const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (slider);

        nameLabel.setText (withLetterSpacing (name), juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setColour (juce::Label::textColourId, StringLifeLookAndFeel::getAccentColour());
        addAndMakeVisible (nameLabel);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));
        StringLifeLookAndFeel::markAsFieldValue (valueLabel);
        addAndMakeVisible (valueLabel);
    };

    setupAdvancedSlider (presenceSlider, presenceNameLabel, presenceValueLabel, "SPARK");
    setupAdvancedSlider (attackSlider, attackNameLabel, attackValueLabel, "BITE");
    setupAdvancedSlider (harmonicSlider, harmonicNameLabel, harmonicValueLabel, "SHIMMER");
    setupAdvancedSlider (sustainSlider, sustainNameLabel, sustainValueLabel, "BLOOM");

    presenceAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::presenceAmount, presenceSlider);
    attackAttachment   = std::make_unique<SliderAttachment> (apvts, ParamIDs::attackRestore, attackSlider);
    harmonicAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::harmonicExcite, harmonicSlider);
    sustainAttachment  = std::make_unique<SliderAttachment> (apvts, ParamIDs::sustainShape, sustainSlider);

    ioTitleLabel.setText (withLetterSpacing ("INPUT / OUTPUT"), juce::dontSendNotification);
    ioTitleLabel.setColour (juce::Label::textColourId, StringLifeLookAndFeel::getSectionTitleColour());
    addAndMakeVisible (ioTitleLabel);

    inKicker.setText (withLetterSpacing ("IN"), juce::dontSendNotification);
    outKicker.setText (withLetterSpacing ("OUT"), juce::dontSendNotification);
    for (auto* l : { &inKicker, &outKicker })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (l);
    }

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    wetDrySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    wetDrySlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (wetDrySlider);
    wetDryAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::wetDryMix, wetDrySlider);

    wetDryLabel.setText (withLetterSpacing ("MIX"), juce::dontSendNotification);
    wetDryLabel.setJustificationType (juce::Justification::centred);
    wetDryLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (wetDryLabel);

    wetDryValueLabel.setJustificationType (juce::Justification::centred);
    wetDryValueLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));
    StringLifeLookAndFeel::markAsFieldValue (wetDryValueLabel);
    addAndMakeVisible (wetDryValueLabel);

#if STRINGLIFE_ENABLE_LICENSING
    licenseOverlay.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (licenseOverlay);

    licenseOverlayTitleLabel.setText ("Unlock StringLife", juce::dontSendNotification);
    licenseOverlayTitleLabel.setJustificationType (juce::Justification::centred);
    licenseOverlayTitleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    licenseOverlay.addAndMakeVisible (licenseOverlayTitleLabel);

    licenseOverlayStatusLabel.setJustificationType (juce::Justification::centred);
    licenseOverlayStatusLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.96f));
    licenseOverlay.addAndMakeVisible (licenseOverlayStatusLabel);

    activateButton.onClick = [this]
    {
        showActivationDialog();
    };
    licenseOverlay.addAndMakeVisible (activateButton);

    trialButton.onClick = [this]
    {
        licenseOverlayDismissed = true;
        updateLicenseOverlayVisibility();
        repaint();
    };
    licenseOverlay.addAndMakeVisible (trialButton);
#endif

    versionLabel.setText ("v" + juce::String (JucePlugin_VersionString), juce::dontSendNotification);
    versionLabel.setColour (juce::Label::textColourId, juce::Colours::grey.withAlpha (0.6f));
    versionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (versionLabel);

    updateInstrumentButtons();
    updateMonitorButtons();

#if STRINGLIFE_ENABLE_LICENSING
    updateLicenseUi();
#endif

    // Resizable with the design aspect ratio locked - resized() scales the
    // whole layout (positions, sizes, fonts) uniformly from the reference
    // design size, so nothing drifts off the background artwork.
    setResizable (true, true);
    setResizeLimits (designWidth * 4 / 5, designHeight * 4 / 5, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth, designHeight);
    startTimerHz (30);
}

StringLifeAudioProcessorEditor::~StringLifeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StringLifeAudioProcessorEditor::setChoiceParam (const juce::String& paramID, int choiceIndex)
{
    if (auto* param = apvts.getParameter (paramID))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) choiceIndex));
}

void StringLifeAudioProcessorEditor::updateFonts (float scale)
{
    // Sizes below are copied straight from the mockup's own CSS (Mockup/
    // stringlife-ui.html + the shared moth-ui.css it imports) - the
    // background art is a literal 1:1 render of that same 820-wide canvas,
    // so a design-space point size here is the same number the stylesheet
    // uses in px.
    manufacturerLabel.setFont (StringLifeFonts::uiLight (12.0f * scale));
    titleLabel.setFont (StringLifeFonts::title (33.0f * scale));
    taglineLabel.setFont (StringLifeFonts::uiLight (11.0f * scale));
    versionLabel.setFont (StringLifeFonts::uiLight (11.0f * scale));

    stringAgeTitleLabel.setFont (StringLifeFonts::title (13.0f * scale));
    macroControlKicker.setFont (StringLifeFonts::uiMedium (9.5f * scale));
    revivalKicker.setFont (StringLifeFonts::uiMedium (9.5f * scale));
    stringAgeValueLabel.setFont (StringLifeFonts::numeric (32.0f * scale));

    for (auto* l : { &deadLabel, &balancedLabel, &freshLabel })
        l->setFont (StringLifeFonts::uiMedium (10.0f * scale));

    for (auto* l : { &presenceNameLabel, &attackNameLabel, &harmonicNameLabel, &sustainNameLabel })
        l->setFont (StringLifeFonts::uiMedium (10.0f * scale));

    for (auto* l : { &presenceValueLabel, &attackValueLabel, &harmonicValueLabel, &sustainValueLabel })
        l->setFont (StringLifeFonts::numeric (12.5f * scale));

    ioTitleLabel.setFont (StringLifeFonts::title (13.0f * scale));

    for (auto* l : { &inKicker, &outKicker })
        l->setFont (StringLifeFonts::uiMedium (9.5f * scale));

    monitorKicker.setFont (StringLifeFonts::uiMedium (9.5f * scale));
    wetDryLabel.setFont (StringLifeFonts::uiMedium (9.5f * scale));
    wetDryValueLabel.setFont (StringLifeFonts::numeric (14.5f * scale));

#if STRINGLIFE_ENABLE_LICENSING
    licenseOverlayTitleLabel.setFont (StringLifeFonts::title (20.0f * scale));
    licenseOverlayStatusLabel.setFont (StringLifeFonts::uiLight (11.0f * scale));
#endif
}

void StringLifeAudioProcessorEditor::updateInstrumentButtons()
{
    auto isBass = apvts.getRawParameterValue (ParamIDs::instrument)->load() > 0.5f;
    guitarButton.setToggleState (! isBass, juce::dontSendNotification);
    bassButton.setToggleState (isBass, juce::dontSendNotification);
}

void StringLifeAudioProcessorEditor::updateMonitorButtons()
{
    auto mode = (MonitorMode) juce::roundToInt (apvts.getRawParameterValue (ParamIDs::monitorMode)->load());
    auto bypassOn = apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;

    normalButton.setToggleState (mode == MonitorMode::normal, juce::dontSendNotification);
    deltaButton.setToggleState  (mode == MonitorMode::delta,  juce::dontSendNotification);

    // The wet/dry knob only has an audible effect in Normal mode, and only
    // when Bypass isn't overriding everything - dim it otherwise so it's
    // clear it's not doing anything. (setEnabled() alone already mutes
    // JUCE's own rotary slider colours, so stacking a low setAlpha() on top
    // of that made it nearly invisible.)
    auto wetDryActive = mode == MonitorMode::normal && ! bypassOn;
    wetDrySlider.setEnabled (wetDryActive);
    wetDryLabel.setEnabled (wetDryActive);
    wetDrySlider.setAlpha (wetDryActive ? 1.0f : 0.7f);
    wetDryLabel.setAlpha (wetDryActive ? 1.0f : 0.7f);

    // Nothing below Bypass matters while it's engaged - disable it visually.
    normalButton.setEnabled (! bypassOn);
    deltaButton.setEnabled (! bypassOn);
    abButton.setEnabled (! bypassOn);
}

void StringLifeAudioProcessorEditor::updateAdvancedLabel (juce::Label& valueLabel, juce::Slider& slider)
{
    valueLabel.setText (juce::String (juce::roundToInt (slider.getValue() * 100.0)) + "%",
                         juce::dontSendNotification);
}

void StringLifeAudioProcessorEditor::timerCallback()
{
    // The mockup's A/B button label is a static "A / B" (only its glow state
    // toggles), unlike the button text approach this editor used before -
    // keep the constant text set once at construction, nothing to update here.
    auto ageValue = apvts.getRawParameterValue (ParamIDs::stringAge)->load();
    stringAgeValueLabel.setText (juce::String (juce::roundToInt (ageValue * 100.0f)) + "%",
                                  juce::dontSendNotification);

    updateAdvancedLabel (presenceValueLabel, presenceSlider);
    updateAdvancedLabel (attackValueLabel, attackSlider);
    updateAdvancedLabel (harmonicValueLabel, harmonicSlider);
    updateAdvancedLabel (sustainValueLabel, sustainSlider);

    wetDryValueLabel.setText (juce::String (juce::roundToInt (wetDrySlider.getValue())) + "%",
                              juce::dontSendNotification);

    updateInstrumentButtons();
    updateMonitorButtons();

#if STRINGLIFE_ENABLE_LICENSING
    updateLicenseUi();
#endif
}

#if STRINGLIFE_ENABLE_LICENSING
void StringLifeAudioProcessorEditor::updateLicenseUi()
{
    if (auto* processor = dynamic_cast<StringLifeAudioProcessor*> (getAudioProcessor()))
    {
        auto& manager = processor->getLicenseManager();
        const auto status = manager.getStatus();

        if (status == licensing::LicenseStatus::TrialActive)
            licenseOverlayStatusLabel.setText (juce::String (manager.getDaysRemainingInTrial()) + " trial day(s) remaining",
                                               juce::dontSendNotification);
        else
            licenseOverlayStatusLabel.setText (manager.getUserVisibleStatusMessage(), juce::dontSendNotification);

        if (status == licensing::LicenseStatus::Activated || status == licensing::LicenseStatus::OfflineGracePeriod)
            licenseOverlayDismissed = false;

        updateLicenseOverlayVisibility();
    }
}

void StringLifeAudioProcessorEditor::updateLicenseOverlayVisibility()
{
    auto visible = false;

    if (auto* processor = dynamic_cast<StringLifeAudioProcessor*> (getAudioProcessor()))
    {
        const auto status = processor->getLicenseManager().getStatus();
        visible = status != licensing::LicenseStatus::Activated
               && status != licensing::LicenseStatus::OfflineGracePeriod
               && ! licenseOverlayDismissed;
    }

    for (auto* component : { (juce::Component*) &licenseOverlayTitleLabel,
                             (juce::Component*) &licenseOverlayStatusLabel,
                             (juce::Component*) &activateButton,
                             (juce::Component*) &trialButton })
        component->setVisible (visible);

    licenseOverlay.setVisible (visible);

    if (visible)
        licenseOverlay.toFront (false);
}

void StringLifeAudioProcessorEditor::showActivationDialog()
{
    auto* processor = dynamic_cast<StringLifeAudioProcessor*> (getAudioProcessor());

    if (processor == nullptr)
        return;

    auto dialog = std::make_unique<juce::AlertWindow> ("Activate StringLife",
                                                       "Enter your license key.",
                                                       juce::AlertWindow::NoIcon);
    dialog->addTextEditor ("licenseKey", {}, "License Key");
    dialog->addButton ("Activate", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, releasedDialog = dialog.release(), processor] (int result)
    {
        std::unique_ptr<juce::AlertWindow> ownedDialog (releasedDialog);

        if (result != 1)
            return;

        const auto key = ownedDialog->getTextEditorContents ("licenseKey").trim();

        if (key.isEmpty())
            return;

        processor->getLicenseManager().startActivation (key, [this] (const licensing::LicenseManager::OperationResult&)
        {
            licenseOverlayDismissed = false;
            updateLicenseUi();
            repaint();
        });
    }));
}
#endif

void StringLifeAudioProcessorEditor::drawPanelFrame (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (backgroundImage.isValid())
    {
        g.drawImage (backgroundImage, bounds);
        return;
    }

    // Procedural fallback, in case the embedded asset ever fails to load.
    juce::ColourGradient bg (juce::Colour (0xff2a2d32), bounds.getX(), bounds.getY(),
                             juce::Colour (0xff0e0f11), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (bounds, panelCornerSize);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), panelCornerSize, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), panelCornerSize, 1.0f);

    auto drawScrew = [&g] (juce::Point<float> centre)
    {
        constexpr float r = 4.5f;
        juce::ColourGradient grad (juce::Colour (0xff6a6d73), centre.x - r, centre.y - r,
                                   juce::Colour (0xff1c1e21), centre.x + r, centre.y + r, false);
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (centre.x - r * 0.6f, centre.y + r * 0.3f, centre.x + r * 0.6f, centre.y - r * 0.3f, 1.2f);
    };

    constexpr float inset = 15.0f;
    drawScrew ({ inset, inset });
    drawScrew ({ bounds.getWidth() - inset, inset });
    drawScrew ({ inset, bounds.getHeight() - inset });
    drawScrew ({ bounds.getWidth() - inset, bounds.getHeight() - inset });
}

void StringLifeAudioProcessorEditor::drawLogo (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (! logoImage.isValid())
        return;

    g.drawImage (logoImage, bounds, juce::RectanglePlacement::centred);
}

void StringLifeAudioProcessorEditor::drawStringArt (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Decorative backdrop behind the Revival readout - a handful of gently
    // undulating "guitar string" lines plus two thin diagonal glow lines,
    // echoing the mockup's .strings/.string-art without reproducing its
    // exact SVG path data.
    if (bounds.isEmpty())
        return;

    auto b = bounds.toFloat();
    auto accent = StringLifeLookAndFeel::getAccentColour();

    g.saveState();
    juce::Path clip;
    clip.addRectangle (b);
    g.reduceClipRegion (clip);

    juce::Random rng (0x53746c66); // fixed seed - stable across repaints/resizes

    for (int i = 0; i < 5; ++i)
    {
        auto yFrac = 0.16f + (float) i * 0.17f;
        auto baseY = b.getY() + b.getHeight() * yFrac;
        auto amp = b.getHeight() * (0.05f + rng.nextFloat() * 0.05f);

        juce::Path line;
        line.startNewSubPath (b.getX(), baseY);

        constexpr int segments = 4;
        for (int s = 0; s < segments; ++s)
        {
            auto x1 = b.getX() + b.getWidth() * ((float) s + 0.5f) / (float) segments;
            auto x2 = b.getX() + b.getWidth() * (float) (s + 1) / (float) segments;
            auto y1 = baseY + amp * (rng.nextFloat() * 2.0f - 1.0f);
            auto y2 = baseY + amp * (rng.nextFloat() * 2.0f - 1.0f);
            line.quadraticTo (x1, y1, x2, y2);
        }

        g.setColour (accent.withAlpha (0.10f + (float) (4 - i) * 0.015f));
        g.strokePath (line, juce::PathStrokeType (1.2f));
    }

    // Two brighter diagonal accent lines, matching .strings::before/::after.
    auto drawAccentLine = [&] (float yFrac, float rotationDeg)
    {
        auto y = b.getY() + b.getHeight() * yFrac;
        auto x1 = b.getX() + b.getWidth() * 0.08f;
        auto x2 = b.getX() + b.getWidth() * 0.92f;

        juce::Path line;
        line.startNewSubPath (x1, y);
        line.lineTo (x2, y);
        line.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rotationDeg),
                                                               b.getCentreX(), b.getCentreY()));

        for (int i = 3; i >= 1; --i)
        {
            g.setColour (accent.withAlpha (0.05f / (float) i));
            g.strokePath (line, juce::PathStrokeType (1.0f + (float) i * 2.0f));
        }

        g.setColour (accent.withAlpha (0.55f));
        g.strokePath (line, juce::PathStrokeType (1.0f));
    };

    drawAccentLine (0.39f, -3.0f);
    drawAccentLine (0.58f, 2.0f);

    g.restoreState();
}

void StringLifeAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Everything visible is either the background artwork itself (with its
    // own printed panel boxes, wells and left-edge accent bar) or a real
    // component/decoration laid on top - no extra drawn dividers or panels.
    drawPanelFrame (g);
    drawStringArt (g, stringArtBounds);
    drawLogo (g, logoBounds);
}

void StringLifeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Assets/background_panel.png is a literal 1:1 render of the mockup's
    // own CSS grid (Mockup/stringlife-ui.html, 820x626) - so every
    // design-space pixel value below is copied straight from that
    // stylesheet (header height, content padding, grid-template-rows/
    // columns, panel padding, knob/field sizes) rather than measured or
    // guessed, and px() just scales that same 820-wide coordinate space to
    // whatever size the (aspect-ratio-locked) window currently is.
    auto scale = (float) bounds.getWidth() / (float) designWidth;
    auto px = [&] (float designPx) { return juce::roundToInt (designPx * scale); };

    updateFonts (scale);

    // ---- Header (moth-header: height 78, padding 0 34) -------------------
    auto header = bounds.removeFromTop (px (78));
    auto headerInner = header.reduced (px (34), 0);

    manufacturerLabel.setBounds (headerInner.removeFromLeft (px (180)));

    auto headerActions = headerInner.removeFromRight (px (250));
    bypassButton.setBounds (headerActions.removeFromRight (px (78)).reduced (0, px (24)));
    headerActions.removeFromRight (px (12));
    abButton.setBounds (headerActions.removeFromRight (px (70)).reduced (0, px (24)));
    headerActions.removeFromRight (px (12));
    versionLabel.setBounds (headerActions.removeFromRight (px (70)));

    logoBounds = juce::Rectangle<float> ((float) px (46), (float) px (32)).withCentre (header.toFloat().getCentre());

    // ---- Content area (moth-content: padding 12 18 56) --------------------
    auto content = bounds.reduced (px (18), 0);
    content.removeFromTop (px (12));
    content.removeFromBottom (px (56));

    // ---- life-layout (grid-template-rows: 68px 1fr 120px; gap: 10px) -----
    auto titleRow    = content.removeFromTop (px (68));
    content.removeFromTop (px (10));
    auto mainRow     = content.removeFromTop (px (272));
    content.removeFromTop (px (10));
    auto advancedRow = content;

    // ---- life-title (columns: 1fr auto 1fr; padding 0 12) -----------------
    auto titleInner = titleRow.reduced (px (12), 0);

    auto monitorBlock = titleInner.removeFromRight (px (150));
    auto instrumentBlock = juce::Rectangle<int> (px (150), px (31)).withCentre (titleRow.getCentre());
    auto productBlock = titleInner;

    titleLabel.setBounds (productBlock.removeFromTop (px (40)));
    productBlock.removeFromTop (px (2));
    taglineLabel.setBounds (productBlock.removeFromTop (px (18)));

    guitarButton.setBounds (instrumentBlock.removeFromLeft (instrumentBlock.getWidth() / 2).reduced (px (3)));
    bassButton.setBounds (instrumentBlock.reduced (px (3)));

    monitorKicker.setBounds (monitorBlock.removeFromTop (px (14)));
    monitorBlock.removeFromTop (px (5));
    auto monitorModeRow = monitorBlock.removeFromTop (px (31));
    normalButton.setBounds (monitorModeRow.removeFromLeft (monitorModeRow.getWidth() / 2).reduced (px (3)));
    deltaButton.setBounds (monitorModeRow.reduced (px (3)));

    // ---- main-row (columns: 1fr 164px; gap: 10px) --------------------------
    auto ioPanel = mainRow.removeFromRight (px (164));
    mainRow.removeFromRight (px (10));
    auto agePanel = mainRow;

    // age-panel (padding: 18 24; rows: auto 1fr 82px)
    auto ageInner = agePanel.reduced (px (24), px (18));
    auto ageHead = ageInner.removeFromTop (px (26));
    stringAgeTitleLabel.setBounds (ageHead.removeFromLeft (ageHead.getWidth() / 2));
    macroControlKicker.setBounds (ageHead);

    auto ageControlRow = ageInner.removeFromBottom (px (82));
    stringArtBounds = ageInner;

    auto readoutBlock = juce::Rectangle<int> (px (160), px (58)).withCentre (ageInner.getCentre());
    revivalKicker.setBounds (readoutBlock.removeFromTop (px (16)));
    stringAgeValueLabel.setBounds (readoutBlock);

    auto rangeRow = ageControlRow.removeFromTop (px (42));
    stringAgeSlider.setBounds (rangeRow);
    ageControlRow.removeFromTop (px (7));
    auto rangeLabelsRow = ageControlRow.removeFromTop (px (15));
    auto rangeLabelThird = rangeLabelsRow.getWidth() / 3;
    deadLabel.setBounds (rangeLabelsRow.removeFromLeft (rangeLabelThird));
    freshLabel.setBounds (rangeLabelsRow.removeFromRight (rangeLabelThird));
    balancedLabel.setBounds (rangeLabelsRow);

    // side-panel / Input-Output (padding: 16 14; rows: auto 1fr 100px)
    auto ioInner = ioPanel.reduced (px (14), px (16));
    ioTitleLabel.setBounds (ioInner.removeFromTop (px (22)));
    auto mixBlock = ioInner.removeFromBottom (px (100));
    auto metersArea = ioInner;

    auto layoutMeter = [&] (juce::Rectangle<int> wrap, LevelMeter& meter, juce::Label& kicker)
    {
        auto meterBounds = juce::Rectangle<int> (px (19), px (82)).withCentre (wrap.getCentre());
        meterBounds.setY (wrap.getY());
        meter.setBounds (meterBounds);
        kicker.setBounds (wrap.getX(), meterBounds.getBottom() + px (5), wrap.getWidth(), px (14));
    };

    auto inWrap  = metersArea.removeFromLeft (metersArea.getWidth() / 2);
    auto outWrap = metersArea;
    layoutMeter (inWrap, inputMeter, inKicker);
    layoutMeter (outWrap, outputMeter, outKicker);

    wetDryLabel.setBounds (mixBlock.removeFromTop (px (14)));
    mixBlock.removeFromTop (px (4));
    auto mixKnobRow = mixBlock.removeFromTop (px (52));
    wetDrySlider.setBounds (juce::Rectangle<int> (px (52), px (52)).withCentre (mixKnobRow.getCentre()));
    mixBlock.removeFromTop (px (4));
    wetDryValueLabel.setBounds (juce::Rectangle<int> (px (58), px (27)).withCentre (mixBlock.getCentre()));

    // ---- advanced-row (4 macro panels; columns: repeat(4,1fr); gap 9px) ---
    auto gap9 = px (9);
    auto colWidth = (advancedRow.getWidth() - gap9 * 3) / 4;

    auto layoutMacro = [&] (juce::Rectangle<int> col, juce::Label& name, juce::Slider& slider, juce::Label& value)
    {
        auto inner = col.reduced (px (10), px (9));
        auto stack = juce::Rectangle<int> (inner.getWidth(), px (97)).withCentre (inner.getCentre());

        name.setBounds (stack.removeFromTop (px (14)));
        stack.removeFromTop (px (1));
        auto knobRow = stack.removeFromTop (px (60));
        slider.setBounds (juce::Rectangle<int> (px (60), px (60)).withCentre (knobRow.getCentre()));
        stack.removeFromTop (px (1));
        value.setBounds (juce::Rectangle<int> (px (53), px (21)).withCentre (stack.getCentre()));
    };

    auto macroCol = advancedRow;
    layoutMacro (macroCol.removeFromLeft (colWidth), presenceNameLabel, presenceSlider, presenceValueLabel);
    macroCol.removeFromLeft (gap9);
    layoutMacro (macroCol.removeFromLeft (colWidth), attackNameLabel, attackSlider, attackValueLabel);
    macroCol.removeFromLeft (gap9);
    layoutMacro (macroCol.removeFromLeft (colWidth), harmonicNameLabel, harmonicSlider, harmonicValueLabel);
    macroCol.removeFromLeft (gap9);
    layoutMacro (macroCol, sustainNameLabel, sustainSlider, sustainValueLabel);

#if STRINGLIFE_ENABLE_LICENSING
    auto fullBounds = getLocalBounds();
    licenseOverlay.setBounds (fullBounds);

    // Proportional to the window regardless of design-space base, so this
    // secondary dialog (not part of the mockup itself) keeps the same look
    // it always had rather than drifting when designWidth/Height changed.
    auto overlayArea = fullBounds.reduced (juce::roundToInt (fullBounds.getWidth() * 0.196f),
                                           juce::roundToInt (fullBounds.getHeight() * 0.32f));
    licenseOverlayTitleLabel.setBounds (overlayArea.getX() + px (16), overlayArea.getY() + px (18),
                                        overlayArea.getWidth() - px (32), px (26));
    licenseOverlayStatusLabel.setBounds (overlayArea.getX() + px (20), overlayArea.getY() + px (54),
                                         overlayArea.getWidth() - px (40), px (34));

    auto buttonRow = juce::Rectangle<int> (overlayArea.getX() + px (38), overlayArea.getBottom() - px (54),
                                           overlayArea.getWidth() - px (76), px (28));
    auto buttonGap = px (10);
    auto buttonWidth = (buttonRow.getWidth() - buttonGap) / 2;
    activateButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (buttonGap);
    trialButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
#endif
}
