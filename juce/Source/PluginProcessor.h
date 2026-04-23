#pragma once
// ACID-303 JUCE — Plugin Processor
//
// JUCE AudioProcessor that owns the synth engine, exposes all parameters
// via AudioProcessorValueTreeState, and handles MIDI input.

#include <JuceHeader.h>
#include "AcidSynthEngine.h"

class Acid303Processor : public juce::AudioProcessor
{
public:
    Acid303Processor();
    ~Acid303Processor() override = default;

    // ── AudioProcessor interface ──────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── Parameter tree ────────────────────────────────────────
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

    // ── Engine access for editor ──────────────────────────────
    acid303::AcidSynthEngine& getEngine() { return engine_; }

private:
    acid303::AcidSynthEngine engine_;
    juce::AudioProcessorValueTreeState apvts_;

    // Atomic parameter pointers for audio-thread read
    std::atomic<float>* cutoffParam_      = nullptr;
    std::atomic<float>* resonanceParam_   = nullptr;
    std::atomic<float>* envModParam_      = nullptr;
    std::atomic<float>* decayParam_       = nullptr;
    std::atomic<float>* accentParam_      = nullptr;
    std::atomic<float>* tuningParam_      = nullptr;
    std::atomic<float>* waveformParam_    = nullptr;
    std::atomic<float>* distOnParam_      = nullptr;
    std::atomic<float>* distAmountParam_  = nullptr;
    std::atomic<float>* delayOnParam_     = nullptr;
    std::atomic<float>* delayTimeParam_   = nullptr;
    std::atomic<float>* delayFbParam_     = nullptr;
    std::atomic<float>* delayMixParam_    = nullptr;
    std::atomic<float>* reverbOnParam_    = nullptr;
    std::atomic<float>* reverbDecayParam_ = nullptr;
    std::atomic<float>* reverbMixParam_   = nullptr;
    std::atomic<float>* lfoRateParam_     = nullptr;
    std::atomic<float>* lfoAmountParam_   = nullptr;
    std::atomic<float>* lfoWaveParam_     = nullptr;
    std::atomic<float>* tempoParam_       = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Acid303Processor)
};
