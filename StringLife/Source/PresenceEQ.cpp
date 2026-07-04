#include "PresenceEQ.h"

void PresenceEQ::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    filter.prepare (spec);
    updateCoefficients();
}

void PresenceEQ::reset()
{
    filter.reset();
}

void PresenceEQ::setBassMode (bool shouldUseBassRange) noexcept
{
    if (bassMode == shouldUseBassRange)
        return;

    bassMode = shouldUseBassRange;
    coefficientsDirty = true;
}

void PresenceEQ::setAmount (float newAmount) noexcept
{
    if (juce::approximatelyEqual (amount, newAmount))
        return;

    amount = newAmount;
    coefficientsDirty = true;
}

void PresenceEQ::updateCoefficients()
{
    auto freqHz = bassMode ? 2500.0f : 5000.0f;
    auto maxGainDb = bassMode ? 5.0f : 6.0f;
    auto gainDb = amount * maxGainDb;
    auto gainFactor = juce::Decibels::decibelsToGain (gainDb);

    // NOTE: makeHighShelf() heap-allocates a new Coefficients object, so this
    // is only called when amount/bassMode actually change (guarded by
    // coefficientsDirty), not on every block.
    // TODO: replace with a coefficient calculation that writes directly into
    // pre-allocated storage for full real-time safety on parameter automation.
    *filter.state = *Coefficients::makeHighShelf (sampleRate, freqHz, 0.707f, gainFactor);
}

void PresenceEQ::process (juce::dsp::AudioBlock<float>& block)
{
    if (coefficientsDirty)
    {
        updateCoefficients();
        coefficientsDirty = false;
    }

    juce::dsp::ProcessContextReplacing<float> context (block);
    filter.process (context);
}
