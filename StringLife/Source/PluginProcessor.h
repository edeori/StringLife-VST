#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

#include "TransientEnhancer.h"
#include "HarmonicExciter.h"
#include "PresenceEQ.h"
#include "UpwardExpander.h"

namespace ParamIDs
{
    inline constexpr auto stringAge      = "stringAge";
    inline constexpr auto instrument     = "instrument";
    inline constexpr auto presenceAmount = "presenceAmount";
    inline constexpr auto attackRestore  = "attackRestore";
    inline constexpr auto harmonicExcite = "harmonicExcite";
    inline constexpr auto sustainShape   = "sustainShape";
    inline constexpr auto wetDryMix      = "wetDryMix";
    inline constexpr auto monitorMode    = "monitorMode";
    inline constexpr auto abCompare      = "abCompare";
    inline constexpr auto bypass         = "bypass";
}

enum class MonitorMode
{
    normal = 0,
    delta = 1
};

class StringLifeAudioProcessor  : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    StringLifeAudioProcessor();
    ~StringLifeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };

    float getInputLevelDb() const noexcept  { return inputLevelDb.load(); }
    float getOutputLevelDb() const noexcept { return outputLevelDb.load(); }

    // Lets hosts show/automate our own bypass parameter instead of (or as
    // well as) their generic per-track bypass.
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return apvts.getParameter (ParamIDs::bypass);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // AudioProcessorValueTreeState::Listener
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void applyMacroToLinkedParameters (float stringAgeValue);

    // A/B compare: each side is a full snapshot of every "sound" parameter,
    // not just a dry/wet toggle - flipping the button saves the currently
    // live values into the side you're leaving and recalls the other side's
    // stored values, like the A/B compare on a hardware channel strip.
    struct SoundSnapshot
    {
        float stringAge  = 0.5f;
        float instrument = 0.0f;
        float presence   = 0.5f;
        float attack     = 0.5f;
        float harmonic   = 0.5f;
        float sustain    = 0.5f;
        float wetDry     = 100.0f;
    };

    SoundSnapshot captureCurrentSnapshot() const;
    void applySnapshot (const SoundSnapshot& snapshot);

    SoundSnapshot snapshotA, snapshotB;

    TransientEnhancer transientEnhancer;
    HarmonicExciter   harmonicExciter;
    PresenceEQ        presenceEQ;
    UpwardExpander    upwardExpander;

    juce::AudioBuffer<float> dryBuffer;

    enum AdvancedParam { presence = 0, attack, harmonic, sustain, numAdvancedParams };

    // Each advanced parameter stays permanently linked to the stringAge macro
    // via a signed offset (paramValue - stringAgeValue at the moment it was
    // last hand-tweaked), rather than a one-shot value copy. Moving the macro
    // shifts every parameter by the same amount, preserving whatever manual
    // differences you dialled in - like a synth macro with relative/offset
    // mapping. Values are clamped to [0, 1] when the macro pushes a parameter
    // past its ceiling/floor, but the offset itself is left untouched, so the
    // parameter "catches up" and keeps tracking as soon as the macro moves
    // back into range.
    std::array<std::atomic<float>, (size_t) numAdvancedParams> macroOffset { 0.0f, 0.0f, 0.0f, 0.0f };

    // Guards macro-driven parameter pushes so parameterChanged() can tell them
    // apart from genuine user/automation edits (which should update the
    // offset above instead).
    // TODO: parameterChanged() can fire from the audio thread under host
    // automation; for full real-time safety the macro push should be
    // deferred (e.g. via AsyncUpdater) rather than calling
    // setValueNotifyingHost() synchronously.
    std::atomic<bool> updatingFromMacro { false };

    std::atomic<float>* stringAgeParam  = nullptr;
    std::atomic<float>* instrumentParam = nullptr;
    std::atomic<float>* presenceParam   = nullptr;
    std::atomic<float>* attackParam     = nullptr;
    std::atomic<float>* harmonicParam   = nullptr;
    std::atomic<float>* sustainParam    = nullptr;
    std::atomic<float>* wetDryParam     = nullptr;
    std::atomic<float>* monitorParam    = nullptr;
    std::atomic<float>* bypassParam     = nullptr;

    std::atomic<float> inputLevelDb  { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringLifeAudioProcessor)
};
