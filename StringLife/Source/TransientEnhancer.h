#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// Envelope-follower based transient enhancer: tracks the input envelope and
// boosts the signal on rising transients (pick/pluck attacks), restoring the
// punch that dead strings lose.
// TODO: replace the naive envelope-derivative detector with a proper dual
// envelope (fast/slow) design and add lookahead for cleaner attack detection.
class TransientEnhancer
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // amount corresponds to the attackRestore parameter (0-1).
    void setAmount (float newAmount) noexcept { amount = newAmount; }

    void process (juce::dsp::AudioBlock<float>& block) noexcept;

private:
    float processSample (int channel, float x) noexcept;

    double sampleRate = 44100.0;
    float amount = 0.5f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    std::vector<float> envelope;
    std::vector<float> prevEnvelope;
};
