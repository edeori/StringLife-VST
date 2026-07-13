#include "PluginEditor.h"
#include "StringLifeFonts.h"
#include "BinaryData.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>

namespace
{
    constexpr float panelCornerSize = 10.0f;

    // Reference window size the design-space pixel values in resized() are
    // measured at - the window is resizable with this aspect ratio locked,
    // and every position, size and font scales uniformly from here.
    constexpr int designWidth  = 560;
    constexpr int designHeight = 500;

    // The Monitor and In/Out boxes are recessed rounded rects baked directly
    // into the background panel artwork, not something we draw ourselves -
    // these fractions are the pixel bounds of those two boxes measured
    // directly from Assets/background_panel.png (a 1448x1086 source image),
    // so the real components we lay on top can be positioned to land
    // exactly inside the printed frame instead of drifting off it.
    constexpr float bottomBoxTopFrac      = 667.0f  / 1086.0f;
    constexpr float bottomBoxBottomFrac   = 928.0f  / 1086.0f;
    constexpr float monitorBoxLeftFrac    = 120.0f  / 1448.0f;
    constexpr float monitorBoxRightFrac   = 591.0f  / 1448.0f;
    constexpr float meterBoxLeftFrac      = 1038.0f / 1448.0f;
    constexpr float meterBoxRightFrac     = 1325.0f / 1448.0f;

    // The In/Out box also has its own baked-in header strip and two vertical
    // meter slots (with a gap between them, and a wider margin after the
    // second slot) - measured the same way as the outer boxes above, so the
    // meter components sit exactly inside their printed cutouts instead of
    // overflowing past them.
    constexpr float meterHeaderBottomFrac = 723.0f  / 1086.0f;
    constexpr float meterSlot1LeftFrac    = 1065.0f / 1448.0f;
    constexpr float meterSlot1RightFrac   = 1165.0f / 1448.0f;
    constexpr float meterSlot2LeftFrac    = 1199.0f / 1448.0f;
    constexpr float meterSlot2RightFrac   = 1298.0f / 1448.0f;
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

    g.setColour (juce::Colour (0xff0c0d0f));
    g.fillRoundedRectangle (bounds, 2.0f);

    auto normalise = [] (float db)
    {
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    };

    auto levelFraction = normalise (displayedLevelDb);
    auto meterBounds = bounds.withTop (bounds.getBottom() - bounds.getHeight() * levelFraction);
    g.setColour (StringLifeLookAndFeel::getAccentColour());
    g.fillRoundedRectangle (meterBounds, 2.0f);

    auto peakY = bounds.getBottom() - bounds.getHeight() * normalise (peakHoldDb);
    g.setColour (juce::Colours::white);
    g.fillRect (bounds.getX(), peakY - 1.0f, bounds.getWidth(), 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
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

    guitarButton.setClickingTogglesState (false);
    bassButton.setClickingTogglesState (false);
    StringLifeLookAndFeel::markAsInstrumentButton (guitarButton);
    StringLifeLookAndFeel::markAsInstrumentButton (bassButton);
    guitarButton.onClick = [this] { setChoiceParam (ParamIDs::instrument, 0); };
    bassButton.onClick   = [this] { setChoiceParam (ParamIDs::instrument, 1); };
    addAndMakeVisible (guitarButton);
    addAndMakeVisible (bassButton);

    deadLabel.setText (withLetterSpacing ("DEAD"), juce::dontSendNotification);
    freshLabel.setText (withLetterSpacing ("FRESH"), juce::dontSendNotification);
    freshLabel.setJustificationType (juce::Justification::centredRight);
    for (auto* l : { &deadLabel, &freshLabel })
    {
        l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (l);
    }

    stringAgeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stringAgeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (stringAgeSlider);
    stringAgeAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::stringAge, stringAgeSlider);

    stringAgeValueLabel.setJustificationType (juce::Justification::centred);
    stringAgeValueLabel.setColour (juce::Label::textColourId, StringLifeLookAndFeel::getAccentColour());
    addAndMakeVisible (stringAgeValueLabel);

    auto setupAdvancedSlider = [this] (juce::Slider& slider, juce::Label& nameLabel, juce::Label& valueLabel,
                                        const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (slider);

        nameLabel.setText (withLetterSpacing (name), juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (nameLabel);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setColour (juce::Label::textColourId, StringLifeLookAndFeel::getAccentColour());
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

    monitorLabel.setText (withLetterSpacing ("MONITOR"), juce::dontSendNotification);
    monitorLabel.setJustificationType (juce::Justification::centred);
    monitorLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (monitorLabel);

    normalButton.setClickingTogglesState (false);
    deltaButton.setClickingTogglesState (false);
    normalButton.onClick = [this] { setChoiceParam (ParamIDs::monitorMode, 0); };
    deltaButton.onClick  = [this] { setChoiceParam (ParamIDs::monitorMode, 1); };
    addAndMakeVisible (normalButton);
    addAndMakeVisible (deltaButton);

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::bypass, bypassButton);

    abButton.setClickingTogglesState (true);
    StringLifeLookAndFeel::markAsAlwaysActive (abButton);
    addAndMakeVisible (abButton);
    abAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::abCompare, abButton);

    wetDrySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    wetDrySlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (wetDrySlider);
    wetDryAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::wetDryMix, wetDrySlider);

    wetDryLabel.setText (withLetterSpacing ("MIX"), juce::dontSendNotification);
    wetDryLabel.setJustificationType (juce::Justification::centred);
    wetDryLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (wetDryLabel);

    metersLabel.setText (withLetterSpacing ("IN / OUT"), juce::dontSendNotification);
    metersLabel.setJustificationType (juce::Justification::centred);
    metersLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (metersLabel);

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

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
    updateCompareButtonLabel();

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
    manufacturerLabel.setFont (StringLifeFonts::uiLight (10.0f * scale));
    titleLabel.setFont (StringLifeFonts::title (28.0f * scale));
    versionLabel.setFont (StringLifeFonts::uiLight (9.0f * scale));

    for (auto* l : { &deadLabel, &freshLabel })
        l->setFont (StringLifeFonts::uiMedium (13.0f * scale));

    stringAgeValueLabel.setFont (StringLifeFonts::numeric (16.0f * scale));

    for (auto* l : { &presenceNameLabel, &attackNameLabel, &harmonicNameLabel, &sustainNameLabel })
        l->setFont (StringLifeFonts::uiMedium (12.0f * scale));

    for (auto* l : { &presenceValueLabel, &attackValueLabel, &harmonicValueLabel, &sustainValueLabel })
        l->setFont (StringLifeFonts::numeric (13.0f * scale));

    for (auto* l : { &monitorLabel, &wetDryLabel, &metersLabel })
        l->setFont (StringLifeFonts::uiMedium (10.5f * scale));

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

void StringLifeAudioProcessorEditor::updateCompareButtonLabel()
{
    auto abOn = apvts.getRawParameterValue (ParamIDs::abCompare)->load() > 0.5f;
    abButton.setButtonText (abOn ? "B" : "A");
}

void StringLifeAudioProcessorEditor::updateAdvancedLabel (juce::Label& valueLabel, juce::Slider& slider)
{
    valueLabel.setText (juce::String (juce::roundToInt (slider.getValue() * 100.0)) + "%",
                         juce::dontSendNotification);
}

void StringLifeAudioProcessorEditor::timerCallback()
{
    auto ageValue = apvts.getRawParameterValue (ParamIDs::stringAge)->load();
    stringAgeValueLabel.setText (juce::String (juce::roundToInt (ageValue * 100.0f)) + "% FRESH",
                                  juce::dontSendNotification);

    updateAdvancedLabel (presenceValueLabel, presenceSlider);
    updateAdvancedLabel (attackValueLabel, attackSlider);
    updateAdvancedLabel (harmonicValueLabel, harmonicSlider);
    updateAdvancedLabel (sustainValueLabel, sustainSlider);

    updateInstrumentButtons();
    updateMonitorButtons();
    updateCompareButtonLabel();

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

void StringLifeAudioProcessorEditor::drawMeterScale (juce::Graphics& g)
{
    static const std::array<int, 7> dbMarks { 0, -6, -12, -18, -24, -36, -60 };

    auto uiScale = (float) getWidth() / (float) designWidth;

    g.setColour (juce::Colours::grey.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (7.0f * uiScale)));

    auto top = (float) meterScaleBounds.getY();
    auto bottom = (float) meterScaleBounds.getBottom();
    auto rowHeight = juce::roundToInt (10.0f * uiScale);

    // The gap between the slots is only ~14px wide (at design size), so a
    // small font and no ellipsis - otherwise "-60" renders as "…".
    for (auto db : dbMarks)
    {
        auto frac = (float) (db + 60) / 60.0f;
        auto y = bottom - frac * (bottom - top);
        g.drawText (juce::String (db), meterScaleBounds.getX(), (int) y - rowHeight / 2, meterScaleBounds.getWidth(), rowHeight,
                    juce::Justification::centred, false);
    }
}

void StringLifeAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Everything visible is either the background artwork itself (with its
    // own printed divider line and recessed boxes) or a real component laid
    // on top - no extra drawn dividers or panels.
    drawPanelFrame (g);
    drawMeterScale (g);
    drawLogo (g, logoBounds);
}

void StringLifeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Everything scales with the window. Background-artwork coordinates
    // (measured on the 1448x1086 source image) map through bgX/bgY as
    // fractions of the current size; design-space paddings and component
    // sizes (laid out for the 560x500 reference window) scale through
    // px(). The aspect ratio is locked by the constrainer, so the two stay
    // in sync and nothing drifts off the printed panel features when the
    // window is resized.
    auto scale = (float) bounds.getWidth() / (float) designWidth;
    auto px  = [&] (float designPx) { return juce::roundToInt (designPx * scale); };
    auto bgX = [&] (float srcX) { return juce::roundToInt (srcX / 1448.0f * (float) bounds.getWidth()); };
    auto bgY = [&] (float srcY) { return juce::roundToInt (srcY / 1086.0f * (float) bounds.getHeight()); };

    updateFonts (scale);

    // Topmost strip (between the bezel and the printed divider line):
    // manufacturer name left, logo centered, version right.
    juce::Rectangle<int> topStrip (bgX (64.0f), bgY (22.0f),
                                   bgX (1380.0f) - bgX (64.0f), bgY (100.0f) - bgY (22.0f));
    manufacturerLabel.setBounds (topStrip.withTrimmedLeft (px (16)).removeFromLeft (px (180)));
    versionLabel.setBounds (topStrip.withTrimmedRight (px (16)).removeFromRight (px (60)));

    constexpr float logoAspect = 592.0f / 380.0f;   // moth_logo.png source proportions
    auto logoHeight = 26.0f * scale;
    logoBounds = juce::Rectangle<float> (logoHeight * logoAspect, logoHeight)
                     .withCentre (topStrip.toFloat().getCentre());

    // Header band between the printed divider and the big content box:
    // title on the left, instrument toggle centered.
    juce::Rectangle<int> headerBand (bgX (64.0f), bgY (100.0f),
                                     bgX (1380.0f) - bgX (64.0f), bgY (263.0f) - bgY (100.0f));
    titleLabel.setBounds (headerBand.withTrimmedLeft (px (16)).removeFromLeft (px (160)));

    auto instrumentArea = juce::Rectangle<int> (px (146), px (32)).withCentre (headerBand.getCentre());
    guitarButton.setBounds (instrumentArea.removeFromLeft (px (70)));
    instrumentArea.removeFromLeft (px (6));
    bassButton.setBounds (instrumentArea);

    // Big content box: string-age slider row tucked into its top, advanced
    // knob row below.
    juce::Rectangle<int> contentBox (bgX (64.0f), bgY (263.0f),
                                     bgX (1380.0f) - bgX (64.0f), bgY (975.0f) - bgY (263.0f));

    auto sliderRow = contentBox.withTrimmedTop (px (12)).removeFromTop (px (30)).reduced (px (20), 0);
    deadLabel.setBounds (sliderRow.removeFromLeft (px (50)));
    freshLabel.setBounds (sliderRow.removeFromRight (px (50)));
    stringAgeSlider.setBounds (sliderRow.reduced (px (6), 0));
    stringAgeValueLabel.setBounds (contentBox.getX(), contentBox.getY() + px (46), contentBox.getWidth(), px (18));

    // Advanced parameter row (Spark | Bite | Shimmer | Bloom).
    auto advancedArea = juce::Rectangle<int> (contentBox.getX() + px (6), contentBox.getY() + px (68),
                                              contentBox.getWidth() - px (12), px (112));
    auto colWidth = advancedArea.getWidth() / 4;

    auto layoutColumn = [&] (juce::Rectangle<int> col, juce::Label& name, juce::Slider& slider, juce::Label& value)
    {
        name.setBounds (col.removeFromTop (px (16)));
        value.setBounds (col.removeFromBottom (px (16)));
        slider.setBounds (col.reduced (px (8), 0));
    };

    layoutColumn (advancedArea.removeFromLeft (colWidth), presenceNameLabel, presenceSlider, presenceValueLabel);
    layoutColumn (advancedArea.removeFromLeft (colWidth), attackNameLabel, attackSlider, attackValueLabel);
    layoutColumn (advancedArea.removeFromLeft (colWidth), harmonicNameLabel, harmonicSlider, harmonicValueLabel);
    layoutColumn (advancedArea, sustainNameLabel, sustainSlider, sustainValueLabel);

    // Bottom section: Monitor box | Mix knob | In/Out box. Anchored to the
    // background artwork's own baked-in box pixels (see the *Frac constants
    // above) rather than flowing from the cumulative layout above, so these
    // components land exactly inside the printed frame regardless of how
    // much vertical space the sections above happened to consume.
    auto boxTop    = juce::roundToInt (bottomBoxTopFrac    * (float) bounds.getHeight());
    auto boxBottom = juce::roundToInt (bottomBoxBottomFrac * (float) bounds.getHeight());

    auto monitorArea = juce::Rectangle<int> (
        juce::roundToInt (monitorBoxLeftFrac * (float) bounds.getWidth()), boxTop,
        juce::roundToInt ((monitorBoxRightFrac - monitorBoxLeftFrac) * (float) bounds.getWidth()),
        boxBottom - boxTop);

    auto meterArea = juce::Rectangle<int> (
        juce::roundToInt (meterBoxLeftFrac * (float) bounds.getWidth()), boxTop,
        juce::roundToInt ((meterBoxRightFrac - meterBoxLeftFrac) * (float) bounds.getWidth()),
        boxBottom - boxTop);

    auto mixArea = juce::Rectangle<int> (monitorArea.getRight(), boxTop,
                                          meterArea.getX() - monitorArea.getRight(), boxBottom - boxTop);

    auto monitorInner = monitorArea.reduced (px (10));
    monitorLabel.setBounds (monitorInner.removeFromTop (px (16)));
    monitorInner.removeFromTop (px (4));
    auto modeRow = monitorInner.removeFromTop ((monitorInner.getHeight() - px (4)) / 2);
    auto modeWidth = modeRow.getWidth() / 2;
    normalButton.setBounds (modeRow.removeFromLeft (modeWidth).reduced (px (3)));
    deltaButton.setBounds  (modeRow.reduced (px (3)));
    monitorInner.removeFromTop (px (4));
    auto overrideRow = monitorInner;
    auto overrideWidth = overrideRow.getWidth() / 2;
    bypassButton.setBounds (overrideRow.removeFromLeft (overrideWidth).reduced (px (3)));
    abButton.setBounds     (overrideRow.reduced (px (3)));

    wetDryLabel.setBounds (mixArea.removeFromTop (px (16)));
    wetDrySlider.setBounds (mixArea.withSizeKeepingCentre (px (76), px (76)));

    // Unlike the Monitor box, the In/Out box has its own baked-in header
    // strip plus two individual meter slot cutouts (with a wider margin
    // after the second slot than before the first) - anchor each piece to
    // its own measured rect rather than splitting meterArea evenly, so the
    // meters land inside their printed slots instead of overflowing them.
    auto meterHeaderBottom = juce::roundToInt (meterHeaderBottomFrac * (float) bounds.getHeight());
    metersLabel.setBounds (juce::Rectangle<int> (meterArea.getX(), boxTop, meterArea.getWidth(), meterHeaderBottom - boxTop));

    auto slot1 = juce::Rectangle<int> (
        juce::roundToInt (meterSlot1LeftFrac * (float) bounds.getWidth()), meterHeaderBottom,
        juce::roundToInt ((meterSlot1RightFrac - meterSlot1LeftFrac) * (float) bounds.getWidth()),
        boxBottom - meterHeaderBottom);

    auto slot2 = juce::Rectangle<int> (
        juce::roundToInt (meterSlot2LeftFrac * (float) bounds.getWidth()), meterHeaderBottom,
        juce::roundToInt ((meterSlot2RightFrac - meterSlot2LeftFrac) * (float) bounds.getWidth()),
        boxBottom - meterHeaderBottom);

    inputMeter.setBounds  (slot1.reduced (px (4)));
    outputMeter.setBounds (slot2.reduced (px (4)));

    // The dB scale goes in the printed gap BETWEEN the two slots - the
    // margins outside the slots are only ~10px wide, which clips "-12" and
    // friends even at a tiny font size.
    meterScaleBounds = juce::Rectangle<int> (slot1.getRight(), meterHeaderBottom,
                                              slot2.getX() - slot1.getRight(), boxBottom - meterHeaderBottom);

#if STRINGLIFE_ENABLE_LICENSING
    licenseOverlay.setBounds (bounds);

    auto overlayArea = bounds.reduced (px (110), px (160));
    licenseOverlayTitleLabel.setBounds (overlayArea.getX() + px (16), overlayArea.getY() + px (18),
                                        overlayArea.getWidth() - px (32), px (26));
    licenseOverlayStatusLabel.setBounds (overlayArea.getX() + px (20), overlayArea.getY() + px (54),
                                         overlayArea.getWidth() - px (40), px (34));

    auto buttonRow = juce::Rectangle<int> (overlayArea.getX() + px (38), overlayArea.getBottom() - px (54),
                                           overlayArea.getWidth() - px (76), px (28));
    auto gap = px (10);
    auto buttonWidth = (buttonRow.getWidth() - gap) / 2;
    activateButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (gap);
    trialButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
#endif
}
