#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// Gentle upward expander applied to note tails: signal below the threshold is
// pulled back up towards it (max ratio 1.2:1), so sustain doesn't die out as
// fast as it does on tired strings. This is the opposite of a downward
// expander/gate - it lifts quiet decaying content rather than attenuating it.
// TODO: replace the simple peak envelope with an RMS detector for smoother,
// less level-jittery behaviour on real recordings.
class UpwardExpander
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // amount corresponds to the sustainShape parameter (0-1).
    void setAmount (float newAmount) noexcept { amount = newAmount; }

    void process (juce::dsp::AudioBlock<float>& block) noexcept;

private:
    float processSample (int channel, float x) noexcept;

    double sampleRate = 44100.0;
    float amount = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    std::vector<float> envelope;

    static constexpr float thresholdDb = -30.0f;
};
