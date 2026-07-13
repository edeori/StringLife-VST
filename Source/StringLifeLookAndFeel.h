#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "StringLifeFonts.h"

// Custom dark/metallic + cyan-glow look, matching the StringLife UI mockup:
// brushed-metal knobs with tick marks and a glowing pointer, a glowing pill
// track for the main macro slider, and bordered toggle buttons that light up
// cyan when engaged.
class StringLifeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StringLifeLookAndFeel();

    // Mockup's own --accent/--accent-hot (Mockup/stringlife-ui.html) - a
    // mint/teal, not the house cyan default other plugins use, since this
    // mockup explicitly overrides it.
    static juce::Colour getAccentColour()    { return juce::Colour (0xff80d7be); }
    static juce::Colour getAccentHotColour() { return juce::Colour (0xffb5ffe9); }

    // moth-section-title colour (near-white, not gold - confirmed directly
    // against the mockup rather than assumed).
    static juce::Colour getSectionTitleColour() { return juce::Colour (0xffd6d9df); }

    // Guitar/Bass and Normal/Delta are both simple two-way pill-tab
    // selectors sharing one visual role in the mockup (.moth-tab) - call
    // this once per button in the editor to opt it into that shared asset
    // pair rather than the plain rect button chrome below.
    static void markAsInstrumentButton (juce::Button& button) { button.getProperties().set (instrumentButtonPropertyKey, true); }

    // For toggles like A/B that don't have a meaningful "off" state (both
    // sides are equally live, it's just a compare switch, not an on/off) -
    // always render with the active/glowing artwork regardless of the
    // button's own toggle state.
    static void markAsAlwaysActive (juce::Button& button) { button.getProperties().set (alwaysActivePropertyKey, true); }

    // Small numeric readouts under the advanced knobs and the Mix knob use
    // a bordered dark "field" box (.moth-field in the mockup), unlike the
    // large free-floating Revival percentage - mark just those labels.
    static void markAsFieldValue (juce::Label& label) { label.getProperties().set (fieldValuePropertyKey, true); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawLabel (juce::Graphics& g, juce::Label& label) override;

private:
    static constexpr const char* instrumentButtonPropertyKey = "stringLifeInstrumentButton";
    static constexpr const char* alwaysActivePropertyKey = "stringLifeAlwaysActive";
    static constexpr const char* fieldValuePropertyKey = "stringLifeFieldValue";

    // Real rendered assets (Assets/*.png, embedded via juce_add_binary_data).
    // The knob face and slider thumb are single fixed-rotation renders, so
    // only the static parts are usable directly - ticks, glow arc and
    // pointer are still drawn procedurally on top so every value renders
    // correctly, not just the ~50% position the source images were baked at.
    juce::Image buttonOnImage, buttonOffImage;
    juce::Image guitarBassOnImage, guitarBassOffImage;
    juce::Image knobFaceImage, sliderThumbImage;
};
