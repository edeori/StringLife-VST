#pragma once

#include <juce_graphics/juce_graphics.h>

// Real embedded typefaces (Assets/Fonts/*.ttf, Google Fonts OFL-licensed),
// so typography looks the same on every machine instead of depending on
// what happens to be installed. Three families, matching the UI mockup's
// typographic mood: Oxanium (bold, wide, techy) for the title, Barlow
// Condensed for everything else, Rajdhani for numeric readouts.
namespace StringLifeFonts
{
    juce::Font title (float size);
    juce::Font uiLight (float size);
    juce::Font uiMedium (float size);
    juce::Font numeric (float size);
}
