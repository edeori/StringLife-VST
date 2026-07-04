#pragma once

#include <juce_dsp/juce_dsp.h>

// High-shelf "presence" boost. Shelf frequency and maximum gain depend on the
// instrument profile: guitar uses a 5kHz shelf up to +6dB, bass uses a 2.5kHz
// shelf up to +5dB.
// TODO: consider a second, narrower peaking band once real material shows
// which specific frequencies need lifting for a convincing "fresh string" tone.
class PresenceEQ
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setBassMode (bool shouldUseBassRange) noexcept;

    // amount corresponds to the presenceAmount parameter (0-1).
    void setAmount (float newAmount) noexcept;

    void process (juce::dsp::AudioBlock<float>& block);

private:
    void updateCoefficients();

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    juce::dsp::ProcessorDuplicator<Filter, Coefficients> filter;

    double sampleRate = 44100.0;
    bool bassMode = false;
    float amount = 0.5f;
    bool coefficientsDirty = true;
};
