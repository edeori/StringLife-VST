#include "PluginProcessor.h"
#include "PluginEditor.h"

StringLifeAudioProcessor::StringLifeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    stringAgeParam  = apvts.getRawParameterValue (ParamIDs::stringAge);
    instrumentParam = apvts.getRawParameterValue (ParamIDs::instrument);
    presenceParam   = apvts.getRawParameterValue (ParamIDs::presenceAmount);
    attackParam     = apvts.getRawParameterValue (ParamIDs::attackRestore);
    harmonicParam   = apvts.getRawParameterValue (ParamIDs::harmonicExcite);
    sustainParam    = apvts.getRawParameterValue (ParamIDs::sustainShape);
    wetDryParam     = apvts.getRawParameterValue (ParamIDs::wetDryMix);
    monitorParam    = apvts.getRawParameterValue (ParamIDs::monitorMode);
    bypassParam     = apvts.getRawParameterValue (ParamIDs::bypass);

    apvts.addParameterListener (ParamIDs::stringAge, this);
    apvts.addParameterListener (ParamIDs::presenceAmount, this);
    apvts.addParameterListener (ParamIDs::attackRestore, this);
    apvts.addParameterListener (ParamIDs::harmonicExcite, this);
    apvts.addParameterListener (ParamIDs::sustainShape, this);
    apvts.addParameterListener (ParamIDs::abCompare, this);

    // Both sides start identical to the parameters' defaults.
    snapshotA = captureCurrentSnapshot();
    snapshotB = captureCurrentSnapshot();
}

StringLifeAudioProcessor::~StringLifeAudioProcessor()
{
    apvts.removeParameterListener (ParamIDs::stringAge, this);
    apvts.removeParameterListener (ParamIDs::presenceAmount, this);
    apvts.removeParameterListener (ParamIDs::attackRestore, this);
    apvts.removeParameterListener (ParamIDs::harmonicExcite, this);
    apvts.removeParameterListener (ParamIDs::sustainShape, this);
    apvts.removeParameterListener (ParamIDs::abCompare, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout StringLifeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::stringAge, 1 }, "String Age",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes().withMeta (true)));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::instrument, 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    // Display names only - internal IDs/state keys are unchanged so the
    // macro-offset and snapshot logic above didn't need touching.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::presenceAmount, 1 }, "Spark",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::attackRestore, 1 }, "Bite",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::harmonicExcite, 1 }, "Shimmer",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::sustainShape, 1 }, "Bloom",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::wetDryMix, 1 }, "Wet/Dry Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::monitorMode, 1 }, "Monitor Mode",
        juce::StringArray { "Normal", "Delta" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::abCompare, 1 }, "A/B Compare", false,
        juce::AudioParameterBoolAttributes().withMeta (true)));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

void StringLifeAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamIDs::stringAge)
    {
        applyMacroToLinkedParameters (newValue);
        return;
    }

    if (parameterID == ParamIDs::abCompare)
    {
        // The parameter has already flipped to newValue by this point, but
        // every other parameter still holds whatever was live on the side
        // being left - so capture it there before recalling the other side.
        auto switchingToB = newValue > 0.5f;
        (switchingToB ? snapshotA : snapshotB) = captureCurrentSnapshot();
        applySnapshot (switchingToB ? snapshotB : snapshotA);
        return;
    }

    // Ignore changes we caused ourselves while pushing the macro value out;
    // anything else is a genuine user/automation edit, which re-anchors that
    // parameter's offset from the macro so future macro moves preserve it.
    if (updatingFromMacro.load())
        return;

    auto currentStringAge = stringAgeParam->load();

    if (parameterID == ParamIDs::presenceAmount)
        macroOffset[(size_t) AdvancedParam::presence] = newValue - currentStringAge;
    else if (parameterID == ParamIDs::attackRestore)
        macroOffset[(size_t) AdvancedParam::attack] = newValue - currentStringAge;
    else if (parameterID == ParamIDs::harmonicExcite)
        macroOffset[(size_t) AdvancedParam::harmonic] = newValue - currentStringAge;
    else if (parameterID == ParamIDs::sustainShape)
        macroOffset[(size_t) AdvancedParam::sustain] = newValue - currentStringAge;
}

StringLifeAudioProcessor::SoundSnapshot StringLifeAudioProcessor::captureCurrentSnapshot() const
{
    SoundSnapshot snap;
    snap.stringAge  = stringAgeParam->load();
    snap.instrument = instrumentParam->load();
    snap.presence   = presenceParam->load();
    snap.attack     = attackParam->load();
    snap.harmonic   = harmonicParam->load();
    snap.sustain    = sustainParam->load();
    snap.wetDry     = wetDryParam->load();
    return snap;
}

void StringLifeAudioProcessor::applySnapshot (const SoundSnapshot& snapshot)
{
    // stringAge first: its automatic macro push (using whatever offsets are
    // currently in memory) runs first, then the four advanced parameters are
    // overwritten with the snapshot's exact values - which also naturally
    // re-derives the correct macroOffset for each via parameterChanged()
    // above, so the snapshot's proportional "feel" comes back too.
    if (auto* param = apvts.getParameter (ParamIDs::stringAge))
        param->setValueNotifyingHost (snapshot.stringAge);

    if (auto* param = apvts.getParameter (ParamIDs::presenceAmount))
        param->setValueNotifyingHost (snapshot.presence);

    if (auto* param = apvts.getParameter (ParamIDs::attackRestore))
        param->setValueNotifyingHost (snapshot.attack);

    if (auto* param = apvts.getParameter (ParamIDs::harmonicExcite))
        param->setValueNotifyingHost (snapshot.harmonic);

    if (auto* param = apvts.getParameter (ParamIDs::sustainShape))
        param->setValueNotifyingHost (snapshot.sustain);

    if (auto* param = apvts.getParameter (ParamIDs::instrument))
        param->setValueNotifyingHost (param->convertTo0to1 (snapshot.instrument));

    if (auto* param = apvts.getParameter (ParamIDs::wetDryMix))
        param->setValueNotifyingHost (param->convertTo0to1 (snapshot.wetDry));
}

void StringLifeAudioProcessor::applyMacroToLinkedParameters (float stringAgeValue)
{
    updatingFromMacro = true;

    auto push = [this, stringAgeValue] (AdvancedParam idx, const char* paramID)
    {
        // Clamped to the parameter's range, but the offset itself is left
        // alone - so if the macro moves back the other way, the parameter
        // resumes tracking it at the same relative distance instead of
        // staying pinned to the clamped value.
        auto target = juce::jlimit (0.0f, 1.0f, stringAgeValue + macroOffset[(size_t) idx].load());

        if (auto* param = apvts.getParameter (paramID))
            param->setValueNotifyingHost (target);
    };

    push (AdvancedParam::presence, ParamIDs::presenceAmount);
    push (AdvancedParam::attack,   ParamIDs::attackRestore);
    push (AdvancedParam::harmonic, ParamIDs::harmonicExcite);
    push (AdvancedParam::sustain,  ParamIDs::sustainShape);

    updatingFromMacro = false;
}

void StringLifeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    auto numChannels = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels };

    transientEnhancer.prepare (sampleRate, numChannels);
    harmonicExciter.prepare (sampleRate, numChannels);
    presenceEQ.prepare (spec);
    upwardExpander.prepare (sampleRate, numChannels);

    dryBuffer.setSize (numChannels, samplesPerBlock);
}

void StringLifeAudioProcessor::releaseResources()
{
    transientEnhancer.reset();
    harmonicExciter.reset();
    presenceEQ.reset();
    upwardExpander.reset();
}

bool StringLifeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainOut == layouts.getMainInputChannelSet();
}

void StringLifeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto numSamples  = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    inputLevelDb.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, numSamples), -100.0f));

    // Hard bypass: true passthrough, skipping all DSP and monitor logic.
    // buffer already holds the unmodified input at this point.
    if (bypassParam->load() > 0.5f)
    {
        outputLevelDb.store (inputLevelDb.load());
        return;
    }

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    auto instrumentIsBass = instrumentParam->load() > 0.5f;
    presenceEQ.setBassMode (instrumentIsBass);
    harmonicExciter.setBassMode (instrumentIsBass);

    transientEnhancer.setAmount (attackParam->load());
    harmonicExciter.setAmount (harmonicParam->load());
    presenceEQ.setAmount (presenceParam->load());
    upwardExpander.setAmount (sustainParam->load());

    juce::dsp::AudioBlock<float> block (buffer);

    transientEnhancer.process (block);
    harmonicExciter.process (block);
    presenceEQ.process (block);
    upwardExpander.process (block);

    // A/B no longer forces a dry passthrough - it's two independent settings
    // snapshots (see applySnapshot()), so both sides get fully processed
    // through the normal monitor mode below, just with different parameters.
    {
        auto monitor = (MonitorMode) juce::roundToInt (monitorParam->load());
        auto mix = juce::jlimit (0.0f, 1.0f, wetDryParam->load() / 100.0f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            auto* dry = dryBuffer.getReadPointer (ch);

            switch (monitor)
            {
                case MonitorMode::normal:
                    for (int i = 0; i < numSamples; ++i)
                        wet[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
                    break;

                case MonitorMode::delta:
                    // +6dB makeup gain so subtle changes are audible.
                    for (int i = 0; i < numSamples; ++i)
                        wet[i] = (wet[i] - dry[i]) * 2.0f;
                    break;
            }
        }
    }

    outputLevelDb.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, numSamples), -100.0f));
}

juce::AudioProcessorEditor* StringLifeAudioProcessor::createEditor()
{
    return new StringLifeAudioProcessorEditor (*this);
}

bool StringLifeAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String StringLifeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StringLifeAudioProcessor::acceptsMidi() const
{
    return false;
}

bool StringLifeAudioProcessor::producesMidi() const
{
    return false;
}

bool StringLifeAudioProcessor::isMidiEffect() const
{
    return false;
}

double StringLifeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StringLifeAudioProcessor::getNumPrograms()
{
    return 1;
}

int StringLifeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StringLifeAudioProcessor::setCurrentProgram (int)
{
}

const juce::String StringLifeAudioProcessor::getProgramName (int)
{
    return {};
}

void StringLifeAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void StringLifeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
    {
        // macroOffset isn't an APVTS parameter (it's derived, runtime-only
        // state), so it has to be persisted by hand - otherwise reloading a
        // session would silently snap every advanced parameter back onto the
        // raw stringAge value, discarding any manual offset from the macro.
        xml->setAttribute ("presenceOffset", macroOffset[(size_t) AdvancedParam::presence].load());
        xml->setAttribute ("attackOffset",   macroOffset[(size_t) AdvancedParam::attack].load());
        xml->setAttribute ("harmonicOffset", macroOffset[(size_t) AdvancedParam::harmonic].load());
        xml->setAttribute ("sustainOffset",  macroOffset[(size_t) AdvancedParam::sustain].load());

        // The currently-active A/B side already lives in the APVTS state
        // above; the other side only exists here, so it has to be saved too
        // or it would silently reset next time the session loads.
        auto writeSnapshot = [&xml] (const juce::String& prefix, const SoundSnapshot& snap)
        {
            xml->setAttribute (prefix + "StringAge",  snap.stringAge);
            xml->setAttribute (prefix + "Instrument", snap.instrument);
            xml->setAttribute (prefix + "Presence",   snap.presence);
            xml->setAttribute (prefix + "Attack",     snap.attack);
            xml->setAttribute (prefix + "Harmonic",   snap.harmonic);
            xml->setAttribute (prefix + "Sustain",    snap.sustain);
            xml->setAttribute (prefix + "WetDry",     snap.wetDry);
        };

        writeSnapshot ("snapA", snapshotA);
        writeSnapshot ("snapB", snapshotB);

        copyXmlToBinary (*xml, destData);
    }
}

void StringLifeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            macroOffset[(size_t) AdvancedParam::presence] = (float) xml->getDoubleAttribute ("presenceOffset", 0.0);
            macroOffset[(size_t) AdvancedParam::attack]   = (float) xml->getDoubleAttribute ("attackOffset", 0.0);
            macroOffset[(size_t) AdvancedParam::harmonic] = (float) xml->getDoubleAttribute ("harmonicOffset", 0.0);
            macroOffset[(size_t) AdvancedParam::sustain]  = (float) xml->getDoubleAttribute ("sustainOffset", 0.0);

            auto readSnapshot = [&xml] (const juce::String& prefix, SoundSnapshot& snap)
            {
                snap.stringAge  = (float) xml->getDoubleAttribute (prefix + "StringAge",  snap.stringAge);
                snap.instrument = (float) xml->getDoubleAttribute (prefix + "Instrument", snap.instrument);
                snap.presence   = (float) xml->getDoubleAttribute (prefix + "Presence",   snap.presence);
                snap.attack     = (float) xml->getDoubleAttribute (prefix + "Attack",     snap.attack);
                snap.harmonic   = (float) xml->getDoubleAttribute (prefix + "Harmonic",   snap.harmonic);
                snap.sustain    = (float) xml->getDoubleAttribute (prefix + "Sustain",    snap.sustain);
                snap.wetDry     = (float) xml->getDoubleAttribute (prefix + "WetDry",     snap.wetDry);
            };

            readSnapshot ("snapA", snapshotA);
            readSnapshot ("snapB", snapshotB);

            // Re-assert the four advanced parameters from the (now correct)
            // offsets, in case the state-restore cascade above triggered our
            // own stringAge listener before its offset was back in place.
            applyMacroToLinkedParameters (stringAgeParam->load());
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StringLifeAudioProcessor();
}
