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

    // ArrayCoefficients computes the raw {b0, b1, b2, a0, a1, a2} set into a
    // stack array, and Coefficients::operator= (assignImpl) normalizes it by
    // a0 into the filter's existing storage. That assign uses clearQuick(),
    // which keeps the Array's capacity, so after the first call (from
    // prepare(), on the message thread) this never allocates - safe to run
    // per-block on the audio thread while the smoothed gain is moving.
    //
    // NB: do NOT copy the raw 6-value array into the storage by hand - the
    // filter stores 5 a0-normalized values, and writing the raw layout in
    // its place produces an unstable filter whose output explodes to Inf
    // (a loud pop, then CoreAudio shuts the output device down).
    *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sampleRate, freqHz, 0.707f, gainFactor);
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
