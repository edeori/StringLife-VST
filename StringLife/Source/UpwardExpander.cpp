#include "UpwardExpander.h"

void UpwardExpander::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    envelope.assign ((size_t) numChannels, 0.0f);

    constexpr float attackMs = 5.0f;
    constexpr float releaseMs = 50.0f;

    attackCoeff  = std::exp (-1.0f / (float) (attackMs  * 0.001 * sampleRate));
    releaseCoeff = std::exp (-1.0f / (float) (releaseMs * 0.001 * sampleRate));
}

void UpwardExpander::reset()
{
    std::fill (envelope.begin(), envelope.end(), 0.0f);
}

float UpwardExpander::processSample (int channel, float x) noexcept
{
    auto absX = std::abs (x);
    auto& env = envelope[(size_t) channel];
    auto coeff = absX > env ? attackCoeff : releaseCoeff;
    env = coeff * env + (1.0f - coeff) * absX;

    auto levelDb = juce::Decibels::gainToDecibels (env, -100.0f);

    if (levelDb >= thresholdDb)
        return x;

    // ratio ranges from 1:1 (no effect) up to 1.2:1 at full amount, shrinking
    // the dB distance below threshold so quiet tails sit closer to it.
    auto ratio = 1.0f + amount * 0.2f;
    auto distanceDb = thresholdDb - levelDb;
    auto newDistanceDb = distanceDb / ratio;
    auto gainDb = distanceDb - newDistanceDb;

    return x * juce::Decibels::decibelsToGain (gainDb);
}

void UpwardExpander::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    if (amount <= 0.0f)
        return;

    auto numChannels = (int) block.getNumChannels();
    auto numSamples  = (int) block.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = block.getChannelPointer ((size_t) ch);

        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (ch, data[i]);
    }
}
