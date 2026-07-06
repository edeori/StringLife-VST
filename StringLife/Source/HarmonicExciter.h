#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// Parallel harmonic exciter: high-passes a copy of the signal, runs it through
// a gentle tanh soft-clipper to generate 2nd/3rd order harmonics, and mixes it
// back in with the dry signal. This restores brightness/sparkle that dead
// strings lose in the upper harmonics.
// TODO: tune the drive curve and harmonic balance once auditioned against
// real guitar/bass recordings; consider splitting even/odd harmonic content.
class HarmonicExciter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // amount corresponds to the harmonicExcite parameter (0-1). It reaches
    // the DSP through a ~20ms smoother, so automation jumps and fast macro
    // sweeps fade instead of zippering.
    void setAmount (float newAmount) noexcept { amountSmoothed.setTargetValue (newAmount); }

    // Selects the high-pass cutoff used to isolate the harmonic-generating band:
    // guitar focuses on 2kHz-12kHz, bass on 800Hz-6kHz.
    void setBassMode (bool shouldUseBassRange) noexcept;

    void process (juce::dsp::AudioBlock<float>& block) noexcept;

private:
    void updateCoefficient();
    float processSample (int channel, float x) noexcept;

    double sampleRate = 44100.0;
    bool bassMode = false;
    float amount = 0.5f;   // per-sample value, fed from amountSmoothed in process()
    juce::SmoothedValue<float> amountSmoothed { 0.5f };
    float hpCoeff = 0.0f;

    std::vector<float> hpPrevInput;
    std::vector<float> hpPrevOutput;
};
