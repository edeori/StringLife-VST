#include "StringLifeFonts.h"
#include "BinaryData.h"

namespace StringLifeFonts
{
    namespace
    {
        juce::Typeface::Ptr loadOnce (const void* data, int size)
        {
            static std::unordered_map<const void*, juce::Typeface::Ptr> cache;

            auto it = cache.find (data);
            if (it != cache.end())
                return it->second;

            auto typeface = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            cache[data] = typeface;
            return typeface;
        }

        juce::Font fromTypeface (juce::Typeface::Ptr typeface, float size)
        {
            return juce::Font (juce::FontOptions (typeface)).withHeight (size);
        }
    }

    juce::Font title (float size)
    {
        return fromTypeface (loadOnce (BinaryData::OxaniumVariable_ttf, BinaryData::OxaniumVariable_ttfSize), size);
    }

    juce::Font uiLight (float size)
    {
        return fromTypeface (loadOnce (BinaryData::BarlowCondensedLight_ttf, BinaryData::BarlowCondensedLight_ttfSize), size);
    }

    juce::Font uiMedium (float size)
    {
        return fromTypeface (loadOnce (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize), size);
    }

    juce::Font numeric (float size)
    {
        return fromTypeface (loadOnce (BinaryData::RajdhaniMedium_ttf, BinaryData::RajdhaniMedium_ttfSize), size);
    }
}
