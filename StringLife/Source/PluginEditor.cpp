#include "PluginEditor.h"
#include "StringLifeFonts.h"
#include "BinaryData.h"
#include <array>

namespace
{
    constexpr float panelCornerSize = 10.0f;

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

    manufacturerLabel.setText (withLetterSpacing ("MOTH PRODUCTION"), juce::dontSendNotification);
    manufacturerLabel.setFont (StringLifeFonts::uiLight (10.0f));
    manufacturerLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (manufacturerLabel);

    titleLabel.setText ("StringLife", juce::dontSendNotification);
    titleLabel.setFont (StringLifeFonts::title (28.0f));
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
        l->setFont (StringLifeFonts::uiMedium (13.0f));
        l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (l);
    }

    stringAgeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stringAgeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (stringAgeSlider);
    stringAgeAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::stringAge, stringAgeSlider);

    stringAgeValueLabel.setJustificationType (juce::Justification::centred);
    stringAgeValueLabel.setFont (StringLifeFonts::numeric (16.0f));
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
        nameLabel.setFont (StringLifeFonts::uiMedium (12.0f));
        nameLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (nameLabel);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setFont (StringLifeFonts::numeric (13.0f));
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
    monitorLabel.setFont (StringLifeFonts::uiMedium (10.5f));
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
    wetDryLabel.setFont (StringLifeFonts::uiMedium (10.5f));
    wetDryLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (wetDryLabel);

    metersLabel.setText (withLetterSpacing ("IN / OUT"), juce::dontSendNotification);
    metersLabel.setJustificationType (juce::Justification::centred);
    metersLabel.setFont (StringLifeFonts::uiMedium (10.5f));
    metersLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (metersLabel);

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    versionLabel.setText ("v" + juce::String (JucePlugin_VersionString), juce::dontSendNotification);
    versionLabel.setFont (StringLifeFonts::uiLight (9.0f));
    versionLabel.setColour (juce::Label::textColourId, juce::Colours::grey.withAlpha (0.6f));
    versionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (versionLabel);

    updateInstrumentButtons();
    updateMonitorButtons();
    updateCompareButtonLabel();

    setSize (560, 500);
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
}

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

    g.setColour (juce::Colours::grey.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (7.0f)));

    auto top = (float) meterScaleBounds.getY();
    auto bottom = (float) meterScaleBounds.getBottom();

    // The gap between the slots is ~14px wide, so a small font and no
    // ellipsis - otherwise "-60" renders as "…".
    for (auto db : dbMarks)
    {
        auto frac = (float) (db + 60) / 60.0f;
        auto y = bottom - frac * (bottom - top);
        g.drawText (juce::String (db), meterScaleBounds.getX(), (int) y - 5, meterScaleBounds.getWidth(), 10,
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

    // Map coordinates measured on the 1448x1086 background artwork into
    // window space, so every section anchors to the printed panel features
    // (topmost strip, its divider line, and the big recessed content box)
    // instead of flowing from whatever the section above happened to use.
    auto bgX = [&] (float srcX) { return juce::roundToInt (srcX / 1448.0f * (float) bounds.getWidth()); };
    auto bgY = [&] (float srcY) { return juce::roundToInt (srcY / 1086.0f * (float) bounds.getHeight()); };

    // Topmost strip (between the bezel and the printed divider line):
    // manufacturer name left, logo centered, version right.
    juce::Rectangle<int> topStrip (bgX (64.0f), bgY (22.0f),
                                   bgX (1380.0f) - bgX (64.0f), bgY (100.0f) - bgY (22.0f));
    manufacturerLabel.setBounds (topStrip.withTrimmedLeft (16).removeFromLeft (180));
    versionLabel.setBounds (topStrip.withTrimmedRight (16).removeFromRight (60));

    constexpr float logoAspect = 592.0f / 380.0f;   // moth_logo.png source proportions
    constexpr float logoHeight = 26.0f;
    logoBounds = juce::Rectangle<float> (logoHeight * logoAspect, logoHeight)
                     .withCentre (topStrip.toFloat().getCentre());

    // Header band between the printed divider and the big content box:
    // title on the left, instrument toggle centered.
    juce::Rectangle<int> headerBand (bgX (64.0f), bgY (100.0f),
                                     bgX (1380.0f) - bgX (64.0f), bgY (263.0f) - bgY (100.0f));
    titleLabel.setBounds (headerBand.withTrimmedLeft (16).removeFromLeft (160));

    auto instrumentArea = juce::Rectangle<int> (146, 32).withCentre (headerBand.getCentre());
    guitarButton.setBounds (instrumentArea.removeFromLeft (70));
    instrumentArea.removeFromLeft (6);
    bassButton.setBounds (instrumentArea);

    // Big content box: string-age slider row tucked into its top, advanced
    // knob row below.
    juce::Rectangle<int> contentBox (bgX (64.0f), bgY (263.0f),
                                     bgX (1380.0f) - bgX (64.0f), bgY (975.0f) - bgY (263.0f));

    auto sliderRow = contentBox.withTrimmedTop (12).removeFromTop (30).reduced (20, 0);
    deadLabel.setBounds (sliderRow.removeFromLeft (50));
    freshLabel.setBounds (sliderRow.removeFromRight (50));
    stringAgeSlider.setBounds (sliderRow.reduced (6, 0));
    stringAgeValueLabel.setBounds (contentBox.getX(), contentBox.getY() + 46, contentBox.getWidth(), 18);

    // Advanced parameter row (Spark | Bite | Shimmer | Bloom).
    auto advancedArea = juce::Rectangle<int> (contentBox.getX() + 6, contentBox.getY() + 68,
                                              contentBox.getWidth() - 12, 112);
    auto colWidth = advancedArea.getWidth() / 4;

    auto layoutColumn = [] (juce::Rectangle<int> col, juce::Label& name, juce::Slider& slider, juce::Label& value)
    {
        name.setBounds (col.removeFromTop (16));
        value.setBounds (col.removeFromBottom (16));
        slider.setBounds (col.reduced (8, 0));
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
    auto fullBounds = getLocalBounds();
    auto boxTop    = juce::roundToInt (bottomBoxTopFrac    * (float) fullBounds.getHeight());
    auto boxBottom = juce::roundToInt (bottomBoxBottomFrac * (float) fullBounds.getHeight());

    auto monitorArea = juce::Rectangle<int> (
        juce::roundToInt (monitorBoxLeftFrac * (float) fullBounds.getWidth()), boxTop,
        juce::roundToInt ((monitorBoxRightFrac - monitorBoxLeftFrac) * (float) fullBounds.getWidth()),
        boxBottom - boxTop);

    auto meterArea = juce::Rectangle<int> (
        juce::roundToInt (meterBoxLeftFrac * (float) fullBounds.getWidth()), boxTop,
        juce::roundToInt ((meterBoxRightFrac - meterBoxLeftFrac) * (float) fullBounds.getWidth()),
        boxBottom - boxTop);

    auto mixArea = juce::Rectangle<int> (monitorArea.getRight(), boxTop,
                                          meterArea.getX() - monitorArea.getRight(), boxBottom - boxTop);

    auto monitorInner = monitorArea.reduced (10);
    monitorLabel.setBounds (monitorInner.removeFromTop (16));
    monitorInner.removeFromTop (4);
    auto modeRow = monitorInner.removeFromTop ((monitorInner.getHeight() - 4) / 2);
    auto modeWidth = modeRow.getWidth() / 2;
    normalButton.setBounds (modeRow.removeFromLeft (modeWidth).reduced (3));
    deltaButton.setBounds  (modeRow.reduced (3));
    monitorInner.removeFromTop (4);
    auto overrideRow = monitorInner;
    auto overrideWidth = overrideRow.getWidth() / 2;
    bypassButton.setBounds (overrideRow.removeFromLeft (overrideWidth).reduced (3));
    abButton.setBounds     (overrideRow.reduced (3));

    wetDryLabel.setBounds (mixArea.removeFromTop (16));
    wetDrySlider.setBounds (mixArea.withSizeKeepingCentre (76, 76));

    // Unlike the Monitor box, the In/Out box has its own baked-in header
    // strip plus two individual meter slot cutouts (with a wider margin
    // after the second slot than before the first) - anchor each piece to
    // its own measured rect rather than splitting meterArea evenly, so the
    // meters land inside their printed slots instead of overflowing them.
    auto meterHeaderBottom = juce::roundToInt (meterHeaderBottomFrac * (float) fullBounds.getHeight());
    metersLabel.setBounds (juce::Rectangle<int> (meterArea.getX(), boxTop, meterArea.getWidth(), meterHeaderBottom - boxTop));

    auto slot1 = juce::Rectangle<int> (
        juce::roundToInt (meterSlot1LeftFrac * (float) fullBounds.getWidth()), meterHeaderBottom,
        juce::roundToInt ((meterSlot1RightFrac - meterSlot1LeftFrac) * (float) fullBounds.getWidth()),
        boxBottom - meterHeaderBottom);

    auto slot2 = juce::Rectangle<int> (
        juce::roundToInt (meterSlot2LeftFrac * (float) fullBounds.getWidth()), meterHeaderBottom,
        juce::roundToInt ((meterSlot2RightFrac - meterSlot2LeftFrac) * (float) fullBounds.getWidth()),
        boxBottom - meterHeaderBottom);

    inputMeter.setBounds  (slot1.reduced (4));
    outputMeter.setBounds (slot2.reduced (4));

    // The dB scale goes in the printed gap BETWEEN the two slots - the
    // margins outside the slots are only ~10px wide, which clips "-12" and
    // friends even at a tiny font size.
    meterScaleBounds = juce::Rectangle<int> (slot1.getRight(), meterHeaderBottom,
                                              slot2.getX() - slot1.getRight(), boxBottom - meterHeaderBottom);
}
