#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Parameter layout ──────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
Acid303Processor::createParameterLayout()
{
    using namespace acid303;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Oscillator
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("waveform", 1), "Waveform",
        juce::StringArray{ "Sawtooth", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("tuning", 1), "Tuning",
        kTuningMin, kTuningMax, kTuningDefault));

    // Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("cutoff", 1), "Cutoff",
        juce::NormalisableRange<float>(kCutoffMin, kCutoffMax, 0.1f, 0.3f),
        kCutoffDefault, "Hz"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resonance", 1), "Resonance",
        juce::NormalisableRange<float>(kResonanceMin, kResonanceMax, 0.1f),
        kResonanceDefault));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("envMod", 1), "Env Mod",
        juce::NormalisableRange<float>(kEnvModMin, kEnvModMax, 0.1f, 0.3f),
        kEnvModDefault, "Hz"));

    // Envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay",
        juce::NormalisableRange<float>(kDecayMin, kDecayMax, 0.01f),
        kDecayDefault, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("accent", 1), "Accent",
        juce::NormalisableRange<float>(kAccentMin, kAccentMax, 0.01f),
        kAccentDefault));

    // Distortion
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("distOn", 1), "Distortion On", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("distAmount", 1), "Drive",
        juce::NormalisableRange<float>(kDistAmountMin, kDistAmountMax, 0.1f),
        kDistAmountDefault));

    // Delay
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("delayOn", 1), "Delay On", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("delayTime", 1), "Delay Time",
        juce::NormalisableRange<float>(kDelayTimeMin, kDelayTimeMax, 0.001f),
        kDelayTimeDefault, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("delayFeedback", 1), "Delay Feedback",
        juce::NormalisableRange<float>(kDelayFeedbackMin, kDelayFeedbackMax, 0.01f),
        kDelayFeedbackDefault));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("delayMix", 1), "Delay Mix",
        juce::NormalisableRange<float>(kDelayMixMin, kDelayMixMax, 0.01f),
        kDelayMixDefault));

    // Reverb
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("reverbOn", 1), "Reverb On", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDecay", 1), "Reverb Decay",
        juce::NormalisableRange<float>(kReverbDecayMin, kReverbDecayMax, 0.01f),
        kReverbDecayDefault, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbMix", 1), "Reverb Mix",
        juce::NormalisableRange<float>(kReverbMixMin, kReverbMixMax, 0.01f),
        kReverbMixDefault));

    // LFO
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfoRate", 1), "LFO Rate",
        juce::NormalisableRange<float>(kLfoRateMin, kLfoRateMax, 0.01f, 0.3f),
        kLfoRateDefault, "Hz"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfoAmount", 1), "LFO Amount",
        juce::NormalisableRange<float>(kLfoAmountMin, kLfoAmountMax, 1.0f),
        kLfoAmountDefault, "ct"));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lfoWave", 1), "LFO Wave",
        juce::StringArray{ "Sine", "Triangle", "Square" }, 0));

    // Sequencer
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("tempo", 1), "Tempo",
        kTempoMin, kTempoMax, kTempoDefault));

    return { params.begin(), params.end() };
}

// ── Constructor ───────────────────────────────────────────────
Acid303Processor::Acid303Processor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
    // Cache raw parameter pointers for audio-thread access
    cutoffParam_      = apvts_.getRawParameterValue("cutoff");
    resonanceParam_   = apvts_.getRawParameterValue("resonance");
    envModParam_      = apvts_.getRawParameterValue("envMod");
    decayParam_       = apvts_.getRawParameterValue("decay");
    accentParam_      = apvts_.getRawParameterValue("accent");
    tuningParam_      = apvts_.getRawParameterValue("tuning");
    waveformParam_    = apvts_.getRawParameterValue("waveform");
    distOnParam_      = apvts_.getRawParameterValue("distOn");
    distAmountParam_  = apvts_.getRawParameterValue("distAmount");
    delayOnParam_     = apvts_.getRawParameterValue("delayOn");
    delayTimeParam_   = apvts_.getRawParameterValue("delayTime");
    delayFbParam_     = apvts_.getRawParameterValue("delayFeedback");
    delayMixParam_    = apvts_.getRawParameterValue("delayMix");
    reverbOnParam_    = apvts_.getRawParameterValue("reverbOn");
    reverbDecayParam_ = apvts_.getRawParameterValue("reverbDecay");
    reverbMixParam_   = apvts_.getRawParameterValue("reverbMix");
    lfoRateParam_     = apvts_.getRawParameterValue("lfoRate");
    lfoAmountParam_   = apvts_.getRawParameterValue("lfoAmount");
    lfoWaveParam_     = apvts_.getRawParameterValue("lfoWave");
    tempoParam_       = apvts_.getRawParameterValue("tempo");
}

// ── Prepare / Release ─────────────────────────────────────────
void Acid303Processor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    engine_.prepare(static_cast<float>(sampleRate));
}

void Acid303Processor::releaseResources() {}

// ── Process block ─────────────────────────────────────────────
void Acid303Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // Sync parameters from APVTS to engine (once per block for efficiency)
    engine_.cutoff    = cutoffParam_->load();
    engine_.resonance = resonanceParam_->load();
    engine_.envMod    = envModParam_->load();
    engine_.decay     = decayParam_->load();
    engine_.accent    = accentParam_->load();
    engine_.tuning    = static_cast<int>(tuningParam_->load());
    engine_.lfoRate   = lfoRateParam_->load();
    engine_.lfoAmount = lfoAmountParam_->load();

    engine_.getOsc().setWaveform(
        waveformParam_->load() < 0.5f
            ? acid303::Waveform::Sawtooth
            : acid303::Waveform::Square);

    engine_.getLfo().setWave(
        static_cast<acid303::LfoWave>(static_cast<int>(lfoWaveParam_->load())));

    engine_.getDistortion().setEnabled(distOnParam_->load() > 0.5f);
    engine_.getDistortion().setAmount(distAmountParam_->load());

    engine_.getDelay().setEnabled(delayOnParam_->load() > 0.5f);
    engine_.getDelay().setDelayTime(delayTimeParam_->load());
    engine_.getDelay().setFeedback(delayFbParam_->load());
    engine_.getDelay().setMix(delayMixParam_->load());

    engine_.getReverb().setEnabled(reverbOnParam_->load() > 0.5f);
    engine_.getReverb().setDecay(reverbDecayParam_->load());
    engine_.getReverb().setMix(reverbMixParam_->load());

    engine_.sequencer.setTempo(static_cast<int>(tempoParam_->load()));

    // Handle MIDI messages
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine_.triggerNote(msg.getNoteNumber(), msg.getVelocity() > 100);
        else if (msg.isNoteOff())
            engine_.releaseNote();
    }

    // Clear buffer and generate audio
    buffer.clear();
    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sample = engine_.processSample();
        left[i] = sample;
        if (right) right[i] = sample;
    }
}

// ── State save/restore ────────────────────────────────────────
void Acid303Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Acid303Processor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Editor ────────────────────────────────────────────────────
juce::AudioProcessorEditor* Acid303Processor::createEditor()
{
    return new Acid303Editor(*this);
}

// ── Plugin entry point ────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Acid303Processor();
}
