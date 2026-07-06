#include "PresenceEQ.h"

void PresenceEQ::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    filter.prepare (spec);

    amountSmoothed.reset (sampleRate, 0.05);
    amountSmoothed.setCurrentAndTargetValue (amountSmoothed.getTargetValue());

    updateCoefficients();
    coefficientsDirty = false;
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
    if (juce::approximatelyEqual (amountSmoothed.getTargetValue(), newAmount))
        return;

    amountSmoothed.setTargetValue (newAmount);
}

void PresenceEQ::updateCoefficients()
{
    auto freqHz = bassMode ? 2500.0f : 5000.0f;
    auto maxGainDb = bassMode ? 5.0f : 6.0f;
    auto gainDb = amountSmoothed.getCurrentValue() * maxGainDb;
    auto gainFactor = juce::Decibels::decibelsToGain (gainDb);

    // ArrayCoefficients computes into a stack array, and the values are then
    // copied into the filter's existing storage - no heap allocation, so
    // this is safe to call from the audio thread every block while the
    // smoothed gain is still moving. (The resize branch only ever runs on
    // the first call, from prepare(), before playback starts.)
    auto coeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sampleRate, freqHz, 0.707f, gainFactor);

    auto& dest = filter.state->coefficients;

    if (dest.size() != (int) coeffs.size())
        dest.resize ((int) coeffs.size());

    for (int i = 0; i < (int) coeffs.size(); ++i)
        dest.setUnchecked (i, coeffs[(size_t) i]);
}

void PresenceEQ::process (juce::dsp::AudioBlock<float>& block)
{
    // Advance the gain smoother at block rate; recompute coefficients only
    // while it's actually moving (or the instrument profile just changed).
    auto stillSmoothing = amountSmoothed.isSmoothing();
    amountSmoothed.skip ((int) block.getNumSamples());

    if (coefficientsDirty || stillSmoothing)
    {
        updateCoefficients();
        coefficientsDirty = false;
    }

    juce::dsp::ProcessContextReplacing<float> context (block);
    filter.process (context);
}
