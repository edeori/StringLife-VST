#include "PluginEditor.h"
#include <cmath>

namespace
{
    juce::String formatSeconds (double seconds)
    {
        if (seconds < 0.0 || std::isnan (seconds) || std::isinf (seconds))
            seconds = 0.0;

        auto totalSeconds = (int) seconds;
        auto mins = totalSeconds / 60;
        auto secs = totalSeconds % 60;
        return juce::String::formatted ("%d:%02d", mins, secs);
    }
}

AudioFilePlayerAudioProcessorEditor::AudioFilePlayerAudioProcessorEditor (AudioFilePlayerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    titleLabel.setText ("AudioFilePlayer", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);

    fileNameLabel.setText ("No file loaded", juce::dontSendNotification);
    fileNameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (fileNameLabel);

    loadButton.onClick = [this] { chooseFile(); };
    addAndMakeVisible (loadButton);

    playButton.onClick = [this] { audioProcessor.play(); };
    addAndMakeVisible (playButton);

    stopButton.onClick = [this] { audioProcessor.stop(); };
    addAndMakeVisible (stopButton);

    addAndMakeVisible (loopButton);
    loopAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::loop, loopButton);

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);

    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible (gainSlider);
    gainAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::gain, gainSlider);

    // Set after the attachment: SliderAttachment installs its own value/text
    // conversion functions (sourced from the parameter's default formatting,
    // which displayed something like "1.0000000"), so ours has to come after
    // to actually stick.
    gainSlider.textFromValueFunction = [] (double v) { return juce::String (v, 2); };
    gainSlider.updateText(); // setValue() no-ops when the value hasn't changed, so force a refresh

    positionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    positionSlider.setRange (0.0, 1.0, 0.001);
    addAndMakeVisible (positionSlider);

    // Only seek on user-driven changes: our own periodic position updates
    // below use dontSendNotification, so they never reach this callback.
    positionSlider.onValueChange = [this] { audioProcessor.seekToSeconds (positionSlider.getValue()); };

    positionLabel.setText ("0:00 / 0:00", juce::dontSendNotification);
    positionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (positionLabel);

    setSize (360, 250);
    startTimerHz (15);
}

AudioFilePlayerAudioProcessorEditor::~AudioFilePlayerAudioProcessorEditor()
{
    stopTimer();
}

void AudioFilePlayerAudioProcessorEditor::chooseFile()
{
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser.launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file.existsAsFile())
        {
            audioProcessor.loadFile (file);
            fileNameLabel.setText (audioProcessor.getLoadedFileName(), juce::dontSendNotification);
        }
    });
}

void AudioFilePlayerAudioProcessorEditor::timerCallback()
{
    playButton.setEnabled (! audioProcessor.isPlaying());
    stopButton.setEnabled (audioProcessor.isPlaying());

    auto lengthSeconds = audioProcessor.getLengthSeconds();
    positionSlider.setRange (0.0, juce::jmax (0.001, lengthSeconds), 0.001);

    // Don't fight the user while they're actively dragging the scrubber.
    if (! positionSlider.isMouseButtonDown())
        positionSlider.setValue (audioProcessor.getCurrentPositionSeconds(), juce::dontSendNotification);

    positionLabel.setText (formatSeconds (audioProcessor.getCurrentPositionSeconds()) + " / "
                             + formatSeconds (lengthSeconds),
                           juce::dontSendNotification);
}

void AudioFilePlayerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey.darker (0.6f));
}

void AudioFilePlayerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);

    fileNameLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (10);

    positionSlider.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);

    auto transportRow = area.removeFromTop (32);
    auto btnWidth = transportRow.getWidth() / 4;
    loadButton.setBounds (transportRow.removeFromLeft (btnWidth).reduced (3));
    playButton.setBounds (transportRow.removeFromLeft (btnWidth).reduced (3));
    stopButton.setBounds (transportRow.removeFromLeft (btnWidth).reduced (3));
    loopButton.setBounds (transportRow.reduced (3));

    area.removeFromTop (10);
    positionLabel.setBounds (area.removeFromTop (20));

    area.removeFromTop (10);
    gainLabel.setBounds (area.removeFromTop (16));
    gainSlider.setBounds (area.removeFromTop (80).withSizeKeepingCentre (80, 80));
}
