#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

namespace ParamIDs
{
    inline constexpr auto gain = "gain";
    inline constexpr auto loop = "loop";
}

// Loads an audio file from disk and plays it back through the output bus.
// Has no audio input and doesn't accept MIDI - it's a source, meant to feed
// another plugin's input (e.g. StringLife) inside a host like AudioPluginHost.
class AudioFilePlayerAudioProcessor  : public juce::AudioProcessor,
                                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    AudioFilePlayerAudioProcessor();
    ~AudioFilePlayerAudioProcessor() override;

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

    // Transport control - called from the editor on the message thread, never
    // from processBlock.
    void loadFile (const juce::File& file);
    void play();
    void stop();
    void setLooping (bool shouldLoop);
    void seekToSeconds (double seconds);

    bool isPlaying() const noexcept   { return transportSource.isPlaying(); }
    double getLengthSeconds() const noexcept;
    double getCurrentPositionSeconds() const noexcept { return transportSource.getCurrentPosition(); }
    juce::String getLoadedFileName() const  { return loadedFile.exists() ? loadedFile.getFileName() : juce::String(); }

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // AudioProcessorValueTreeState::Listener
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread readAheadThread { "AudioFilePlayer read-ahead" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    juce::File loadedFile;

    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* loopParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFilePlayerAudioProcessor)
};
