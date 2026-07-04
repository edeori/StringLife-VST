#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioFilePlayerAudioProcessor::AudioFilePlayerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
    readAheadThread.startThread (juce::Thread::Priority::normal);

    gainParam = apvts.getRawParameterValue (ParamIDs::gain);
    loopParam = apvts.getRawParameterValue (ParamIDs::loop);

    apvts.addParameterListener (ParamIDs::loop, this);
}

AudioFilePlayerAudioProcessor::~AudioFilePlayerAudioProcessor()
{
    apvts.removeParameterListener (ParamIDs::loop, this);

    transportSource.setSource (nullptr);
    readAheadThread.stopThread (1000);
}

void AudioFilePlayerAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamIDs::loop)
        setLooping (newValue > 0.5f);
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioFilePlayerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::gain, 1 }, "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::loop, 1 }, "Loop", false));

    return { params.begin(), params.end() };
}

void AudioFilePlayerAudioProcessor::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    auto* rawReader = formatManager.createReaderFor (file);
    if (rawReader == nullptr)
        return;

    auto sourceSampleRate = rawReader->sampleRate;
    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (rawReader, true);
    newSource->setLooping (loopParam != nullptr && loopParam->load() > 0.5f);

    transportSource.stop();
    transportSource.setSource (newSource.get(), 32768, &readAheadThread, sourceSampleRate);

    readerSource = std::move (newSource);
    loadedFile = file;
}

void AudioFilePlayerAudioProcessor::play()
{
    if (readerSource == nullptr)
        return;

    if (transportSource.hasStreamFinished())
        transportSource.setPosition (0.0);

    transportSource.start();
}

void AudioFilePlayerAudioProcessor::stop()
{
    transportSource.stop();
    transportSource.setPosition (0.0);
}

void AudioFilePlayerAudioProcessor::seekToSeconds (double seconds)
{
    if (readerSource == nullptr)
        return;

    transportSource.setPosition (juce::jlimit (0.0, transportSource.getLengthInSeconds(), seconds));
}

void AudioFilePlayerAudioProcessor::setLooping (bool shouldLoop)
{
    if (readerSource != nullptr)
        readerSource->setLooping (shouldLoop);
}

double AudioFilePlayerAudioProcessor::getLengthSeconds() const noexcept
{
    return transportSource.getLengthInSeconds();
}

void AudioFilePlayerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay (samplesPerBlock, sampleRate);
}

void AudioFilePlayerAudioProcessor::releaseResources()
{
    transportSource.releaseResources();
}

bool AudioFilePlayerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void AudioFilePlayerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    buffer.clear();

    juce::AudioSourceChannelInfo info (&buffer, 0, buffer.getNumSamples());
    transportSource.getNextAudioBlock (info);

    if (gainParam != nullptr)
        buffer.applyGain (gainParam->load());
}

juce::AudioProcessorEditor* AudioFilePlayerAudioProcessor::createEditor()
{
    return new AudioFilePlayerAudioProcessorEditor (*this);
}

bool AudioFilePlayerAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String AudioFilePlayerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioFilePlayerAudioProcessor::acceptsMidi() const
{
    return false;
}

bool AudioFilePlayerAudioProcessor::producesMidi() const
{
    return false;
}

bool AudioFilePlayerAudioProcessor::isMidiEffect() const
{
    return false;
}

double AudioFilePlayerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioFilePlayerAudioProcessor::getNumPrograms()
{
    return 1;
}

int AudioFilePlayerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioFilePlayerAudioProcessor::setCurrentProgram (int)
{
}

const juce::String AudioFilePlayerAudioProcessor::getProgramName (int)
{
    return {};
}

void AudioFilePlayerAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void AudioFilePlayerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
    {
        xml->setAttribute ("loadedFilePath", loadedFile.getFullPathName());
        copyXmlToBinary (*xml, destData);
    }
}

void AudioFilePlayerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            auto path = xml->getStringAttribute ("loadedFilePath");
            if (path.isNotEmpty())
                loadFile (juce::File (path));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFilePlayerAudioProcessor();
}
