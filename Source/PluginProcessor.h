/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array> // add near top with other includes
#include <juce_dsp/juce_dsp.h>

#include "model/AbletonLink.h"
#include "model/JuceState.h"

//==============================================================================
/**
*/
class TickAudioProcessor : public juce::AudioProcessor,
                           private juce::AsyncUpdater
{
public:
    //==============================================================================
    // TAP CC ÄNDRING: Deklarera statisk sträng för parameter-ID
    static const juce::String kTapTempoButtonID; 
    static const juce::String kMidiNoteBeat1ID;
    static const juce::String kMidiNoteOtherID;
    //==============================================================================
    
    TickAudioProcessor();
    ~TickAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    static BusesProperties getDefaultLayout();

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif

    void processBlock (juce::AudioSampleBuffer&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isHostSyncSupported();

    juce::AudioPlayHead::PositionInfo playheadPosition_;

    double getCurrentBeatPos();
    int getBeat() { return uiCurrentBeat.load(std::memory_order_relaxed); }

    TicksHolder& getTicks() { return ticks; }
    TickSettings& getState() { return settings; }

    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters; }

    void setExternalProps (juce::PropertySet* s);

    AbletonLink m_link;

private:
    // TAP CC ÄNDRING: Deklarera metoden för Tap-logik
    bool handleTapTempo(uint32_t nowMs);

    // Nytt: håll koll på föregående state för att undvika dubbel-triggering
    std::atomic<bool> lastTapButtonState { false };
    
    // Hantera parameter-reset säkert för ljudtråden
    void handleAsyncUpdate() override;

    void handlePreCount (double inputPPQ);
    
    // audio samples bank
    TicksHolder ticks;
    
    // programmable assignment of samples per beat.
    // hard-coded to support upto 64.
    TickSettings settings;

    struct TickState
    {
        int currentSample = -1;
        int tickLengthInSamples = 0;
        int tickStartPosition = 0;
        int beat = 0;
        float beatGain = 1.0f;
        double beatPos = 0;
        juce::AudioSampleBuffer sample;
        float* refer[1];
        bool isClear = true;

        void fillTickSample (juce::AudioBuffer<float>& bufferToFill);
        void addTickSample (juce::AudioBuffer<float>& bufferToFill, int startPos, int endPos);
        void clear();
    } tickState;

    juce::dsp::StateVariableTPTFilter<float> lpfFilter;
    float lastCutoffHz { -1.0f };
    float lastMasterGainDb { -1000.0f };
    float masterGainMultiplier { 1.0f };
    
    // DSP-Optimering: En pre-allokerad MIDI-buffer för att undvika minnesallokering (malloc) i ljudtråden
    juce::MidiBuffer passThroughMidi;
    //==============================================================================
    // Parameters
    std::atomic<float> tickMultiplier {1.0f};
    std::atomic<float>* filterCutoff;
    std::atomic<float>* masterGain;
    std::atomic<float>* midiNoteBeat1Param { nullptr };
    std::atomic<float>* midiNoteOtherParam { nullptr };
    juce::AudioParameterBool* tapTempoParam { nullptr };
    juce::AudioProcessorValueTreeState parameters;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TickAudioProcessor)

    // per‑instans debounce timestamp (ms)
    std::atomic<uint32_t> lastTapTimeMs { 0u };

    // MIDI Clock state (för stabil Beat Clock Out)
    double nextClockPpq { 0.0 };
    bool wasPlaying { false };
    
    // Trådsäker UI-variabel för visuell metronom-ritning utan Data Race
    std::atomic<double> uiCurrentBeatPos { 0.0 };
    std::atomic<int> uiCurrentBeat { 1 };

    // Trådsäker överföring från Ljudtråd -> ValueTree (GUI) för att förhindra krascher
    std::atomic<float> uiBpm { -1.0f };
    std::atomic<int> uiNumerator { -1 };
    std::atomic<int> uiDenominator { -1 };
    std::atomic<int> uiIsPlaying { -1 };
    std::atomic<int> uiUseHost { -1 };

    // Variabler för att avgöra om GUI/Preset ändrade inställningarna lokalt
    float lastSettingsBpm { 120.0f };
    bool lastSettingsIsPlaying { true };

    // Skydd för kontinuerliga sustain-pedaler (Half-Damper)
    bool lastMidiCC64State { false };
}; // end class TickAudioProcessor