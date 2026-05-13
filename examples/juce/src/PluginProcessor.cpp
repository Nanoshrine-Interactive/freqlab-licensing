#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "freqlab_licensing.h"
#include "freqlab_response.h"

JucerguyProcessor::JucerguyProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Sync the SDK cache against the server when the plugin loads.
    // refreshAsync() returns immediately and does the blocking HTTPS
    // call on the SDK's worker thread.
    freqlab::licensing::refreshAsync();
}

JucerguyProcessor::~JucerguyProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout JucerguyProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1},
        "Gain",
        juce::NormalisableRange<float>(-30.0f, 30.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; },
        [](const juce::String& text) { return text.getFloatValue(); }
    ));

    return { params.begin(), params.end() };
}

void JucerguyProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    gainParameter = apvts.getRawParameterValue("gain");
    smoothedGain.reset(sampleRate, 0.02);  // 20ms ramp
    smoothedGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(gainParameter->load()));

    juce::ignoreUnused(samplesPerBlock);
}

void JucerguyProcessor::releaseResources()
{
}

// The `@freqlab:require-license` marker tells our cloud transformer to
// wrap this function with a license gate at build time. The inline
// `freqlab_should_run_dsp(buffer)` call below makes the same gate run
// during local development too, so the example silences correctly
// without a cloud build.
//
// @freqlab:require-license
void JucerguyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    if (!freqlab_should_run_dsp(buffer))
        return;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    smoothedGain.setTargetValue(juce::Decibels::decibelsToGain(gainParameter->load()));

    if (smoothedGain.isSmoothing())
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float gain = smoothedGain.getNextValue();
            for (int channel = 0; channel < totalNumInputChannels; ++channel)
            {
                float* channelData = buffer.getWritePointer(channel);
                channelData[sample] *= gain;
                if (!std::isfinite(channelData[sample]))
                    channelData[sample] = 0.0f;
            }
        }
    }
    else
    {
        float gain = smoothedGain.getTargetValue();
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                channelData[sample] *= gain;
                if (!std::isfinite(channelData[sample]))
                    channelData[sample] = 0.0f;
            }
        }
    }
}

void JucerguyProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void JucerguyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* JucerguyProcessor::createEditor()
{
    return new JucerguyEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JucerguyProcessor();
}
