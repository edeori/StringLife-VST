#include "StringLifeFonts.h"
#include "BinaryData.h"

namespace StringLifeFonts
{
    namespace
    {
        juce::Typeface::Ptr loadTypeface (const void* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }

        juce::Font fromTypeface (juce::Typeface::Ptr typeface, float size)
        {
            return juce::Font (juce::FontOptions (typeface)).withHeight (size);
        }
    }

    juce::Font title (float size)
    {
        return fromTypeface (loadTypeface (BinaryData::OxaniumVariable_ttf, BinaryData::OxaniumVariable_ttfSize), size);
    }

    juce::Font uiLight (float size)
    {
        return fromTypeface (loadTypeface (BinaryData::BarlowCondensedLight_ttf, BinaryData::BarlowCondensedLight_ttfSize), size);
    }

    juce::Font uiMedium (float size)
    {
        return fromTypeface (loadTypeface (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize), size);
    }

    juce::Font numeric (float size)
    {
        return fromTypeface (loadTypeface (BinaryData::RajdhaniMedium_ttf, BinaryData::RajdhaniMedium_ttfSize), size);
    }
}
