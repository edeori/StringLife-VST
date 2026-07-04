#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class AudioFilePlayerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit AudioFilePlayerAudioProcessorEditor (AudioFilePlayerAudioProcessor&);
    ~AudioFilePlayerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseFile();

    AudioFilePlayerAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label fileNameLabel;
    juce::Label positionLabel;

    // Seekable timeline: dragged/clicked positions are only sent to the
    // processor while the user is actually holding the slider (see
    // timerCallback()), so our own periodic position updates don't fight
    // the user's scrubbing.
    juce::Slider positionSlider;

    juce::TextButton loadButton  { "Load..." };
    juce::TextButton playButton  { "Play" };
    juce::TextButton stopButton  { "Stop" };
    juce::ToggleButton loopButton { "Loop" };

    juce::Slider gainSlider;
    juce::Label gainLabel;

    juce::FileChooser fileChooser { "Select an audio file to load...", juce::File(),
                                     "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<ButtonAttachment> loopAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFilePlayerAudioProcessorEditor)
};
