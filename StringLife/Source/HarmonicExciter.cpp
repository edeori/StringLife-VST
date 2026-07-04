#include "HarmonicExciter.h"

void HarmonicExciter::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    hpPrevInput.assign ((size_t) numChannels, 0.0f);
    hpPrevOutput.assign ((size_t) numChannels, 0.0f);
    updateCoefficient();
}

void HarmonicExciter::reset()
{
    std::fill (hpPrevInput.begin(), hpPrevInput.end(), 0.0f);
    std::fill (hpPrevOutput.begin(), hpPrevOutput.end(), 0.0f);
}

void HarmonicExciter::setBassMode (bool shouldUseBassRange) noexcept
{
    bassMode = shouldUseBassRange;
    updateCoefficient();
}

void HarmonicExciter::updateCoefficient()
{
    // High-pass cutoff sits near the bottom of the instrument's focus range,
    // isolating the band that gets driven into the waveshaper.
    auto cutoffHz = bassMode ? 800.0 : 2000.0;
    auto rc = 1.0 / (2.0 * juce::MathConstants<double>::pi * cutoffHz);
    hpCoeff = (float) (rc / (rc + 1.0 / sampleRate));
}

float HarmonicExciter::processSample (int channel, float x) noexcept
{
    auto& prevIn  = hpPrevInput[(size_t) channel];
    auto& prevOut = hpPrevOutput[(size_t) channel];

    // One-pole high-pass filter.
    auto hp = hpCoeff * (prevOut + x - prevIn);
    prevIn = x;
    prevOut = hp;

    constexpr float maxDrive = 3.0f; // keeps THD around ~0.3% at amount = 1
    auto driven = std::tanh (hp * (1.0f + amount * maxDrive));

    return x + driven * amount * 0.3f;
}

void HarmonicExciter::process (juce::dsp::AudioBlock<float>& block) noexcept
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
