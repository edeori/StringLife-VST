#include "StringLifeLookAndFeel.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour panelDark      { 0xff17191c };
    const juce::Colour knobHighlight  { 0xffe4e6ea };
    const juce::Colour knobShadow     { 0xff222429 };
    const juce::Colour tickInactive   { 0xff4a4d54 };
}

StringLifeLookAndFeel::StringLifeLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, getAccentColour());
    setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOnId, getAccentColour());
    setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));

    buttonOnImage      = juce::ImageFileFormat::loadFrom (BinaryData::button_on_png, (size_t) BinaryData::button_on_pngSize);
    buttonOffImage     = juce::ImageFileFormat::loadFrom (BinaryData::button_off_png, (size_t) BinaryData::button_off_pngSize);
    guitarBassOnImage  = juce::ImageFileFormat::loadFrom (BinaryData::guitarbass_on_png, (size_t) BinaryData::guitarbass_on_pngSize);
    guitarBassOffImage = juce::ImageFileFormat::loadFrom (BinaryData::guitarbass_off_png, (size_t) BinaryData::guitarbass_off_pngSize);
    knobFaceImage      = juce::ImageFileFormat::loadFrom (BinaryData::knob_face_png, (size_t) BinaryData::knob_face_pngSize);
    sliderThumbImage   = juce::ImageFileFormat::loadFrom (BinaryData::slider_thumb_png, (size_t) BinaryData::slider_thumb_pngSize);
}

void StringLifeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                              juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centre = bounds.getCentre();
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto accent = getAccentColour();
    auto enabled = slider.isEnabled();

    auto knobRadius = radius * 0.72f;
    auto knobBounds = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre);

    // Tick marks around the perimeter.
    constexpr int numTicks = 11;
    for (int i = 0; i < numTicks; ++i)
    {
        auto tickAngle = rotaryStartAngle + (float) i / (float) (numTicks - 1) * (rotaryEndAngle - rotaryStartAngle);
        auto isActive = tickAngle <= angle + 0.001f;
        juce::Point<float> inner (centre.x + (radius * 0.86f) * std::sin (tickAngle), centre.y - (radius * 0.86f) * std::cos (tickAngle));
        juce::Point<float> outer (centre.x + radius * std::sin (tickAngle), centre.y - radius * std::cos (tickAngle));
        g.setColour ((isActive && enabled) ? accent : tickInactive);
        g.drawLine ({ inner, outer }, 1.4f);
    }

    // Knob body: real brushed-metal render if available, else a procedural
    // metallic gradient + drawn pointer fallback. The source image already
    // has a pointer line baked in (pointing straight up, at its ~50%
    // reference render) but no tick ring (cropped out - ours is drawn above
    // instead, so it stays fixed to the panel like a real control's printed
    // scale rather than spinning with the knob cap). We rotate the whole
    // image by the value's angle relative to that 50% reference, which
    // carries the baked pointer to the correct position - so there's only
    // ever one pointer, not a static one plus a drawn one fighting it.
    if (knobFaceImage.isValid())
    {
        auto referenceAngle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
        auto rotationDelta = angle - referenceAngle;

        g.saveState();
        juce::Path circleClip;
        circleClip.addEllipse (knobBounds.reduced (knobBounds.getWidth() * 0.03f));
        g.reduceClipRegion (circleClip);
        g.addTransform (juce::AffineTransform::rotation (rotationDelta, centre.x, centre.y));
        g.setOpacity (enabled ? 1.0f : 0.5f);
        g.drawImage (knobFaceImage, knobBounds);
        g.setOpacity (1.0f);
        g.restoreState();

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (knobBounds.reduced (0.5f), 1.0f);
    }
    else
    {
        juce::ColourGradient bodyGradient (knobHighlight.withMultipliedBrightness (enabled ? 1.0f : 0.6f),
                                           centre.x, knobBounds.getY(),
                                           knobShadow, centre.x, knobBounds.getBottom(), false);
        g.setGradientFill (bodyGradient);
        g.fillEllipse (knobBounds);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (knobBounds, 1.2f);

        // Pointer, with a soft glow when enabled.
        juce::Path pointer;
        auto pointerLength = knobRadius * 0.82f;
        auto pointerThickness = 3.0f;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.88f, pointerThickness * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));

        if (enabled)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.10f / (float) i));
                g.strokePath (pointer, juce::PathStrokeType (pointerThickness + (float) i * 2.5f));
            }
        }

        g.setColour (enabled ? accent : tickInactive);
        g.fillPath (pointer);
    }
}

void StringLifeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float, float,
                                              const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    auto accent = getAccentColour();
    auto enabled = slider.isEnabled();

    auto trackHeight = juce::jmin (10.0f, (float) height * 0.4f);
    auto track = juce::Rectangle<float> ((float) x, (float) y + ((float) height - trackHeight) * 0.5f, (float) width, trackHeight);

    g.setColour (panelDark);
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    auto filled = track.withRight (juce::jlimit (track.getX(), track.getRight(), sliderPos));

    if (enabled)
        g.setColour (accent);
    else
        g.setColour (tickInactive);

    g.fillRoundedRectangle (filled, trackHeight * 0.5f);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (track, trackHeight * 0.5f, 1.0f);

    // Thumb: real glowing metal render if available, else a procedural
    // gradient + glow fallback. The thumb doesn't rotate (this is a linear
    // slider), so the source image can be used directly with no rotation
    // trick needed, unlike the rotary knob face.
    auto thumbRadius = (float) height * 0.42f;
    juce::Point<float> thumbCentre (sliderPos, (float) y + (float) height * 0.5f);
    auto thumbBounds = juce::Rectangle<float> (thumbRadius * 2.0f, thumbRadius * 2.0f).withCentre (thumbCentre);

    if (sliderThumbImage.isValid())
    {
        // The source render is a tight circular crop with a real alpha
        // channel, so it can be drawn directly - just slightly larger than
        // the nominal thumb size so the knob reads as sitting ON the track
        // rather than inside it.
        auto imageBounds = thumbBounds.expanded (thumbRadius * 0.12f);

        g.setOpacity (enabled ? 1.0f : 0.6f);
        g.drawImage (sliderThumbImage, imageBounds);
        g.setOpacity (1.0f);
    }
    else
    {
        if (enabled)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.10f / (float) i));
                g.fillEllipse (thumbBounds.expanded ((float) i * 2.5f));
            }
        }

        juce::ColourGradient thumbGradient (knobHighlight, thumbCentre.x, thumbBounds.getY(),
                                            knobShadow, thumbCentre.x, thumbBounds.getBottom(), false);
        g.setGradientFill (thumbGradient);
        g.fillEllipse (thumbBounds);

        g.setColour (enabled ? accent : tickInactive);
        g.drawEllipse (thumbBounds, 2.0f);
    }
}

namespace
{
    // Stretches the source image to exactly fill dest, clipped to a rounded
    // rect with OUR OWN fixed corner radius - not the source image's own
    // corners. Slicing the image into fixed-size "caps" plus a stretched
    // middle (to preserve the source's native corner radius) was tried
    // first, but on/off source renders don't share one consistent corner
    // geometry, so the two states ended up with visibly different roundness
    // at small button sizes. A flat stretch plus a clip we fully control
    // keeps every button's corners identical regardless of source content.
    void drawButtonImage (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> dest, float cornerSize)
    {
        if (dest.getWidth() <= 0.0f || dest.getHeight() <= 0.0f)
            return;

        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (dest, cornerSize);
        g.reduceClipRegion (clip);
        g.drawImage (image, dest);
        g.restoreState();
    }
}

void StringLifeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                  bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    constexpr float cornerSize = 5.0f;
    auto isOn = button.getToggleState() || button.getProperties().contains (alwaysActivePropertyKey);
    auto accent = getAccentColour();

    auto isInstrumentButton = button.getProperties().contains (instrumentButtonPropertyKey);
    const auto& image = isInstrumentButton ? (isOn ? guitarBassOnImage : guitarBassOffImage)
                                            : (isOn ? buttonOnImage : buttonOffImage);

    if (image.isValid())
    {
        g.setOpacity (button.isEnabled() ? 1.0f : 0.5f);
        drawButtonImage (g, image, bounds, cornerSize);
        g.setOpacity (1.0f);

        if (! isOn && (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted))
        {
            g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsDown ? 0.10f : 0.06f));
            g.fillRoundedRectangle (bounds, cornerSize);
        }

        return;
    }

    // Procedural fallback, in case the embedded assets ever fail to load.
    g.setColour (panelDark);
    g.fillRoundedRectangle (bounds, cornerSize);

    if (isOn)
    {
        g.setColour (accent.withAlpha (0.16f));
        g.fillRoundedRectangle (bounds, cornerSize);

        g.setColour (accent.withAlpha (0.20f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), cornerSize + 1.0f, 1.5f);
    }
    else if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsDown ? 0.08f : 0.05f));
        g.fillRoundedRectangle (bounds, cornerSize);
    }

    g.setColour (isOn ? accent : juce::Colours::grey.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds, cornerSize, isOn ? 1.5f : 1.0f);

    if (! button.isEnabled())
    {
        g.setColour (panelDark.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, cornerSize);
    }
}

void StringLifeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    auto isOn = button.getToggleState() || button.getProperties().contains (alwaysActivePropertyKey);
    g.setColour ((isOn ? getAccentColour() : juce::Colours::white.withAlpha (0.85f))
                    .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

juce::Font StringLifeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    // Purely height-relative (no absolute cap), so button text scales
    // together with the resizable UI.
    return StringLifeFonts::uiMedium ((float) buttonHeight * 0.47f);
}

void StringLifeLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (label.getProperties().contains (fieldValuePropertyKey))
    {
        // .moth-field: dark translucent fill + thin border, rounded corners
        // - the advanced-knob and Mix numeric readouts sit inside one of
        // these in the mockup, unlike the big free-floating Revival value.
        auto bounds = label.getLocalBounds().toFloat();

        g.setColour (juce::Colour (0xff030407).withAlpha (0.82f));
        g.fillRoundedRectangle (bounds, 3.0f);

        g.setColour (juce::Colour (0xff303540));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
    }

    LookAndFeel_V4::drawLabel (g, label);
}
