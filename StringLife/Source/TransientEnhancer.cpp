#include "TransientEnhancer.h"

void TransientEnhancer::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    envelope.assign ((size_t) numChannels, 0.0f);
    prevEnvelope.assign ((size_t) numChannels, 0.0f);

    constexpr float attackMs = 0.5f;
    constexpr float releaseMs = 20.0f;

    attackCoeff  = std::exp (-1.0f / (float) (attackMs  * 0.001 * sampleRate));
    releaseCoeff = std::exp (-1.0f / (float) (releaseMs * 0.001 * sampleRate));
}

void TransientEnhancer::reset()
{
    std::fill (envelope.begin(), envelope.end(), 0.0f);
    std::fill (prevEnvelope.begin(), prevEnvelope.end(), 0.0f);
}

float TransientEnhancer::processSample (int channel, float x) noexcept
{
    auto absX = std::abs (x);
    auto& env = envelope[(size_t) channel];
    auto coeff = absX > env ? attackCoeff : releaseCoeff;
    env = coeff * env + (1.0f - coeff) * absX;

    auto& prevEnv = prevEnvelope[(size_t) channel];
    auto transient = env - prevEnv;
    prevEnv = env;

    // Only ride the gain up on rising transients, leave decays untouched.
    auto boost = juce::jmax (0.0f, transient) * amount * 8.0f;
    return x + x * boost;
}

void TransientEnhancer::process (juce::dsp::AudioBlock<float>& block) noexcept
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
