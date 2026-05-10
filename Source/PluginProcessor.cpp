/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "utils/UtilityFunctions.h"

#include <atomic>

using namespace juce;

// NOTE: removed global g_lastTapTimeMs; per-instance lastTapTimeMs används i PluginProcessor.h

const juce::String TickAudioProcessor::kTapTempoButtonID = "tap_tempo";
const juce::String TickAudioProcessor::kMidiNoteBeat1ID = "midiNoteBeat1";
const juce::String TickAudioProcessor::kMidiNoteOtherID = "midiNoteOther";
//==============================================================================

AudioProcessor::BusesProperties TickAudioProcessor::getDefaultLayout()
{
    // workaround to Ableton Live 10
    if (PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_VST3)
        return BusesProperties()
            .withInput ("Input", AudioChannelSet::stereo(), true)
            .withOutput ("Output", AudioChannelSet::stereo(), true);

    return BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput ("Input", AudioChannelSet::stereo(), true)
#endif
        .withOutput ("Output", AudioChannelSet::stereo(), true)
#endif
        ;
}

//==============================================================================
TickAudioProcessor::TickAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (getDefaultLayout())
#endif
      ,
      settings (ticks),
      parameters (*this, nullptr, Identifier (JucePlugin_Name), { 
          std::make_unique<AudioParameterFloat> (ParameterID (IDs::filterCutoff.toString(), 1),
                                                  "Filter Cutoff",
                                                  TickUtils::makeLogarithmicRange<float> (100.0, 20000.0f),
                                                  20000.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (auto val, auto)
                                                                                                               { return String (roundToInt (val)) + "Hz"; })
                                                      .withLabel ("Hz")),
          std::make_unique<AudioParameterFloat> (ParameterID (IDs::masterGain.toString(), 1),
                                                  "Master Gain",
                                                  NormalisableRange<float> (-60.0f, 6.0f),
                                                  0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (auto val, auto)
                                                                                                               { return String (roundToInt (val)) + "dB"; })
                                                      .withLabel ("dB")),
                        
          // TAP CC ÄNDRING: Tap-parametern som nytt element
          std::make_unique<juce::AudioParameterBool> (ParameterID (kTapTempoButtonID, 1), 
                                                      "Tap", 
                                                      false),
          std::make_unique<juce::AudioParameterInt> (ParameterID (kMidiNoteBeat1ID, 1), 
                                                     "MIDI Note (Beat 1)", 
                                                     0, 127, 34),
          std::make_unique<juce::AudioParameterInt> (ParameterID (kMidiNoteOtherID, 1), 
                                                     "MIDI Note (Other)", 
                                                     0, 127, 33)
      })
{
    // init samples reading
    ticks.clear();


    filterCutoff = parameters.getRawParameterValue (IDs::filterCutoff.toString());
    masterGain = parameters.getRawParameterValue (IDs::masterGain.toString());
    tapTempoParam = dynamic_cast<juce::AudioParameterBool*> (parameters.getParameter (kTapTempoButtonID));
    midiNoteBeat1Param = parameters.getRawParameterValue (kMidiNoteBeat1ID);
    midiNoteOtherParam = parameters.getRawParameterValue (kMidiNoteOtherID);

    // load default preset
    setStateInformation (BinaryData::factory_default_preset, BinaryData::factory_default_presetSize);

    settings.useHostTransport.setValue (wrapperType != WrapperType::wrapperType_Standalone, nullptr);
    playheadPosition_ = juce::AudioPlayHead::PositionInfo();
    settings.isDirty = false;

    // Default till Play-läge direkt vid inladdning
    settings.transport.isPlaying.setValue(true, nullptr);
    uiIsPlaying.store(1, std::memory_order_relaxed);
}

TickAudioProcessor::~TickAudioProcessor()
{
    tickState.clear();
    ticks.clear();
}

void TickAudioProcessor::setExternalProps (juce::PropertySet* s)
{
    static_cast<TickAudioProcessorEditor*> (getActiveEditor())->standaloneProps = s;
}

//==============================================================================
const String TickAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TickAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool TickAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool TickAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double TickAudioProcessor::getTailLengthSeconds() const
{
    return 2.0;
}

int TickAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int TickAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TickAudioProcessor::setCurrentProgram (int /*index*/)
{
}

const String TickAudioProcessor::getProgramName (int /*index*/)
{
    return {};
}

void TickAudioProcessor::changeProgramName (int /*index*/, const String& /*newName*/)
{
}

//==============================================================================
void TickAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Säkerhetsspärr: Garantera alltid en rimlig minimum sample rate vid allokering för att
    // undvika att minnesbuffertar blir för små vid DAW-quirks eller byten av ljudkort!
    const double safeSr = juce::jmax(44100.0, sampleRate);
    getState().samplerate = safeSr;
    ticks.setSampleRate (safeSr);
    tickState.clear();

    // Preallokera upp till 10 sekunder för tick-samplet för att undvika 'makeCopyOf' (malloc) i on the fly
    tickState.sample.setSize(1, (int)(safeSr * 10.0));

    // Förbered det nya juce::dsp-filtret
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = safeSr;
    // FIX: Allokera filtret efter DAW:ens aktuella buffertstorlek, inte 10 sekunder. 
    // Det sparar massivt med onödigt RAM-minne (upp till flera megabyte per instans)!
    spec.maximumBlockSize = (uint32_t)juce::jmax(1, samplesPerBlock);
    // FIX: Sätt kanalantalet till dynamiskt (oftast 2 för Stereo) för att undvika minneskrasch i filtret
    spec.numChannels = (uint32_t)juce::jmax(1, getTotalNumOutputChannels());
    lpfFilter.prepare(spec);
    lpfFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    lpfFilter.reset();
    lastCutoffHz = -1.0f;
}

void TickAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TickAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // standalone can assert due to different layouts!
    if (wrapperType == wrapperType_Standalone)
        return true;

#if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

        // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

bool TickAudioProcessor::isHostSyncSupported()
{
    return wrapperType != AudioProcessor::wrapperType_Standalone;
}

void TickAudioProcessor::handlePreCount (const double inputPPQ)
{
    const int preCount = getState().transport.preCount.get();
    if (preCount <= 0 || getState().useHostTransport.get())
        return;
    // auto stop
    const auto ts = playheadPosition_.getTimeSignature().orFallback (AudioPlayHead::TimeSignature ({ 1, 4 }));
    const auto ttq = (4.0 / juce::jmax(1, ts.denominator)); // tick to quarter (säkerhetsspärr)
    const auto expectedBar = std::floor (inputPPQ / ttq / juce::jmax(1, ts.numerator)); // Säkerhetsspärr
    if ((int)expectedBar == preCount)
    {
        uiIsPlaying.store(0, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
}

// NOTE: named midiMessages to allow MIDI inspection
void TickAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{

    ScopedNoDenormals noDenormals; // ENDAST DENNA RAD SKA VARA KVAR

    // 0. OS-Optimering: Hämta aktuell tid en enda gång per ljudblock för att undvika onödiga systemanrop
    const uint32_t blockTimeMs = juce::Time::getMillisecondCounter();

    // 1. Läs in atomics EN gång per block till lokala variabler för max prestanda
    const float currentFilterCutoff = filterCutoff->load(std::memory_order_relaxed);
    const float currentMasterGain = masterGain->load(std::memory_order_relaxed);
    const float currentTickMultiplier = tickMultiplier.load(std::memory_order_relaxed);

    // DSP-Optimering: Läs in MIDI-parametrar EN gång per block istället för varje tick
    const int currentNote1 = midiNoteBeat1Param ? juce::roundToInt(midiNoteBeat1Param->load(std::memory_order_relaxed)) : 34;
    const int currentNoteOther = midiNoteOtherParam ? juce::roundToInt(midiNoteOtherParam->load(std::memory_order_relaxed)) : 33;

    bool forceSongRestart = false;
    bool tapTriggered = false;

    // MIDI logging + robust single-tap detection (one rising-edge per controller, per-debounce)
    if (! midiMessages.isEmpty())
    {
        for (const auto metadata : midiMessages)
        {
            // OS-Optimering: Filtrera bort SysEx, stora paket eller trasig data (över 3 eller under 2 bytes)
            if (metadata.numBytes > 3 || metadata.numBytes < 2)
                continue;
                
            // DSP-Optimering: Läs raw bytes direkt för att undvika instansiering av MidiMessage-objekt!
            const juce::uint8* data = metadata.data;
            const int status = data[0] & 0xF0;
            const int channel = (data[0] & 0x0F) + 1;
            
            // --- NY FUNKTION: Universell BPM-mottagning (Kanal 10, 11 & 12) ---
            // Accepterar Program Change, Note On, och Control Change för att kringgå Cubase-filtrering!
            if (status == 0xC0 || status == 0x90 || status == 0xB0)
            {
                int val = -1;
                if (status == 0xC0) val = data[1]; // Program Change
                else if (status == 0x90 && data[2] > 0) val = data[1]; // Note On (velocity > 0)
                // Säkerhet: Vid CC, lyssna ENDAST på CC-nummer 119 för att förhindra att Mod-wheel/Volym/Sustain ändrar tempot!
                else if (status == 0xB0 && data[1] == 119) val = data[2]; // Control Change (CC)

                if (val > 0)
                {
                    double newBpm = 0.0;
                    if (channel == 10) newBpm = val;
                    else if (channel == 11) newBpm = val + 100.0;
                    else if (channel == 12) newBpm = val + 200.0;

                    if (newBpm > 0.0)
                    {
                    uiUseHost.store(0, std::memory_order_relaxed);
                    uiBpm.store((float)newBpm, std::memory_order_relaxed);
                    uiIsPlaying.store(1, std::memory_order_relaxed);
                    triggerAsyncUpdate();
                    forceSongRestart = true;
                }
            }
        }
        
        // LIVE SHOW OPTIMERING: Rensa bufferten! Hindra inkommande PC/CC-kommandon från
        // att "blöda" igenom VST-pluginen och skickas ut till TouchDesigner.
        midiMessages.clear();
    }

    // 1. Endast trigga på stigande flank (rising edge) för att undvika dubbel-triggering
    if (tapTempoParam)
    {
        const bool curState = tapTempoParam->get();
        const bool prevState = lastTapButtonState.load(std::memory_order_relaxed);

        if (curState && ! prevState)
        {
            // Suppress if global tap debounce hasn't expired
            constexpr uint32_t kTapDebounceMs = 80;
            const uint32_t lastTapMs = lastTapTimeMs.load(std::memory_order_relaxed);
            if (lastTapMs == 0 || (blockTimeMs - lastTapMs) >= kTapDebounceMs)
            {
                if (handleTapTempo(blockTimeMs))
                    tapTriggered = true;
            }

            // reset the parameter on message thread so host doesn't keep reporting it
            triggerAsyncUpdate();

            lastTapButtonState.store (true, std::memory_order_relaxed);
        }
        else if (! curState)
        {
            lastTapButtonState.store (false, std::memory_order_relaxed);
        }
    }

    buffer.clear();

    // LIVE SHOW OPTIMERING: Om vi byter låt (PC) eller tappar in tempot när Cubase rullar
    if (forceSongRestart)
    {
        // TD-Optimering: Skicka 'Stop' innan 'Start' om vi redan spelar. Vissa noder 
        // i TouchDesigner vägrar nollställa sin tidslinje om de inte först får ett Stop-kommando!
        if (wasPlaying)
            midiMessages.addEvent(juce::MidiMessage::midiStop(), 0);

        playheadPosition_.setPpqPosition(0.0);
        playheadPosition_.setTimeInSamples(0);
        playheadPosition_.setTimeInSeconds(0.0);
        nextClockPpq = 0.0;
        midiMessages.addEvent(juce::MidiMessage::midiStart(), 0);
        wasPlaying = true; // Förhindra dubbla midiStart längre ner
    }
    else if (tapTriggered)
    {
        // Fas-aligna klicket med Helix-pedalen!
        double current = playheadPosition_.getPpqPosition().orFallback(0.0);
        double snapped = std::round(current); // FIX: Avrunda till NÄRMASTE slag istället för framåt, behåller naturlig fas
        playheadPosition_.setPpqPosition(snapped);
        nextClockPpq = snapped; // Resynca klockan så den inte spottar ut en "burst" av pulser
    }

    // standalone mode
    // Läs in aktuellt state, och applicera UI-brevlådan DIREKT för noll latens om den har nytt data!
    const bool isUseHost = uiUseHost.load(std::memory_order_relaxed) >= 0 ? uiUseHost.load(std::memory_order_relaxed) == 1 : settings.useHostTransport.get();
    if (! isHostSyncSupported() || ! isUseHost)
    {
#if JUCE_IOS
        AbletonLink::Requests requests;
        if (playheadPosition_.getIsPlaying() != settings.transport.isPlaying.get())
            requests.isPlaying = settings.transport.isPlaying.get();

        if (playheadPosition_.getBpm().hasValue() && *playheadPosition_.getBpm() != settings.transport.bpm.get())
            requests.bpm = settings.transport.bpm.get();
#endif

        const bool isPlayingState = uiIsPlaying.load(std::memory_order_relaxed) >= 0 ? (uiIsPlaying.load(std::memory_order_relaxed) == 1) : static_cast<bool>(settings.transport.isPlaying.get());
        const int numState = uiNumerator.load(std::memory_order_relaxed) >= 0 ? uiNumerator.load(std::memory_order_relaxed) : static_cast<int>(settings.transport.numerator.get());
        const int denumState = uiDenominator.load(std::memory_order_relaxed) >= 0 ? uiDenominator.load(std::memory_order_relaxed) : static_cast<int>(settings.transport.denumerator.get());
        const float bpmState = uiBpm.load(std::memory_order_relaxed) >= 0.0f ? uiBpm.load(std::memory_order_relaxed) : static_cast<float>(settings.transport.bpm.get());

        playheadPosition_.setIsPlaying (isPlayingState);
        playheadPosition_.setTimeSignature(AudioPlayHead::TimeSignature ({numState, denumState}));
        playheadPosition_.setBpm (bpmState);
        if (! playheadPosition_.getIsPlaying())
        {
            playheadPosition_.setPpqPosition({});
            playheadPosition_.setPpqPositionOfLastBarStart({});
            playheadPosition_.setTimeInSamples({});
            playheadPosition_.setTimeInSeconds({});
            tickState.clear();
        }

#if JUCE_IOS
        if (m_link.isLinkConnected())
        {
            m_link.linkPosition (playheadPosition_, requests);
            uiIsPlaying.store(playheadPosition_.getIsPlaying() ? 1 : 0, std::memory_order_relaxed);
            triggerAsyncUpdate();
        }
#endif
    }
    else if (getPlayHead())
    {
        playheadPosition_ = getPlayHead()->getPosition().orFallback(AudioPlayHead::PositionInfo());
    }

    // DSP-Optimering & GUI-Fix: Uppdatera bara ValueTree om värdet har ändrats markant.
    // Att anropa setValue med mikroskopiska flyttalsvariationer varje ljudblock får JUCE att 
    // spamma UI-tråden med uppdateringar, vilket triggar Windows Accessibility (UIA) att stjäla 
    // fönsterfokus så att Cubase-menyerna stängs ner!
    if (playheadPosition_.getBpm().hasValue())
    {
        const float newBpm = (float) *playheadPosition_.getBpm();
        if (std::abs(settings.transport.bpm.get() - newBpm) > 0.01f)
        {
            uiBpm.store(newBpm, std::memory_order_relaxed);
            triggerAsyncUpdate();
        }
    }
    if (playheadPosition_.getTimeSignature().hasValue())
    {
        const auto ts = *playheadPosition_.getTimeSignature();
        if (settings.transport.numerator.get() != ts.numerator)
        {
            uiNumerator.store(ts.numerator, std::memory_order_relaxed);
            triggerAsyncUpdate();
        }
        if (settings.transport.denumerator.get() != ts.denominator)
        {
            uiDenominator.store(ts.denominator, std::memory_order_relaxed);
            triggerAsyncUpdate();
        }
    }

    // --- NY FUNKTION: MIDI Beat Clock Out (Jitter-free) ---
    bool isPlaying = playheadPosition_.getIsPlaying();
    
    // DSP-Optimering: Uppdatera filterkoefficienter oavsett om metronomen spelar
    // (annars uppdateras inte filtret när du skruvar på det medan DAW:en står stilla!)
    // Säkerhet: Begränsa cutoff till Nyquist-frekvensen (Halva Sample Rate) för att förhindra krasch
    const double actualSr = juce::jmax(1.0, getSampleRate());
    const float safeCutoff = (float) juce::jmin((double)currentFilterCutoff, (actualSr / 2.0) - 1.0);
    
    if (lastCutoffHz != safeCutoff)
    {
        lpfFilter.setCutoffFrequency(safeCutoff);
        lastCutoffHz = safeCutoff;
    }

    if (isPlaying && !wasPlaying)
    {
        midiMessages.addEvent(juce::MidiMessage::midiStart(), 0);
        nextClockPpq = playheadPosition_.getPpqPosition().orFallback(0.0);
    }
    else if (!isPlaying && wasPlaying)
    {
        midiMessages.addEvent(juce::MidiMessage::midiStop(), 0);
    }
    wasPlaying = isPlaying;

    if (isPlaying)
    {
        double currentPpq = playheadPosition_.getPpqPosition().orFallback(0.0);
        
        // --- NYTT: Jump Detection & Grid Snapping ---
        // FIX: Snävare hopp-detektion (0.5). Om DAW:en hoppade 0.9 framåt förut skapades en 
        // "burst" av pulser på sample 0. Nu resyncar klockan korrekt vid alla synbara hopp!
        constexpr double pulseInterval = 1.0 / 24.0;
        if (currentPpq < nextClockPpq - pulseInterval || currentPpq > nextClockPpq + 0.5)
        {
            nextClockPpq = std::ceil(currentPpq / pulseInterval) * pulseInterval;
        }

        double bpm = playheadPosition_.getBpm().orFallback(120.0);
        
        // Säkerhetsspärr: Förhindra division med noll (vilket fryser DAW:en) om BPM saknas
        if (bpm <= 0.0) bpm = 120.0;
        
        double qps = bpm / 60.0; // quarters per second
        double sr = juce::jmax(1.0, getSampleRate()); // Säkerhetsspärr mot sr=0
        double samplesPerQuarter = sr / qps;
        
        // Beräkna var i denna buffer (PPQ) vi befinner oss
        double bufEndPpq = currentPpq + (buffer.getNumSamples() / samplesPerQuarter);
        
        // Skicka en klockpuls (0xF8) 24 gånger per fjärdedelsnot
        while (nextClockPpq < bufEndPpq)
        {
            double ppqOffset = nextClockPpq - currentPpq;
            int sampleOffset = juce::roundToInt(ppqOffset * samplesPerQuarter);
            
            // Säkerhet: Förhindra krasch om DAW skickar flush-block med 0 samples
            int maxSampleIndex = juce::jmax(0, buffer.getNumSamples() - 1);
            sampleOffset = juce::jlimit(0, maxSampleIndex, sampleOffset);
            
            midiMessages.addEvent(juce::MidiMessage::midiClock(), sampleOffset);
            nextClockPpq += (1.0 / 24.0);
        }
    }

    if (playheadPosition_.getIsPlaying())
    {
        if (ticks.getLock().try_lock())
        {
        const auto ppqPosition = playheadPosition_.getPpqPosition().orFallback(0);
        const auto lastBarStart = playheadPosition_.getPpqPositionOfLastBarStart().orFallback(0);
        // Säkerhet: Förhindra Integer Overflow-krasch! Om DAW-automationen går nära 0 BPM, 
        // blir antalet samples per slag > 2.14 miljarder vilket kraschar 32-bitars integers.
        const auto bpm = juce::jmax(1.0f, (float)playheadPosition_.getBpm().orFallback(120.0f));
        const auto ts = playheadPosition_.getTimeSignature().orFallback(AudioPlayHead::TimeSignature ({1, 4}));
        
        // calculate where tick starts in samples...
        jassert ((int)lastBarStart == 0 || ppqPosition >= lastBarStart);
        // FIX: Förhindra negativ tid vid floating-point avrundningsfel från DAW:en
        const auto pos = juce::jmax(0.0, ppqPosition - lastBarStart);
        const auto bps = bpm / 60.0;
        const double sr = juce::jmax(1.0, getSampleRate()); // Säkerhetsspärr mot sr=0
        const auto bpSmp = sr / bps;
        const auto ttq = (4.0 / juce::jmax(1, ts.denominator)); // tick to quarter (säkerhetsspärr)
        // Säkerhetsspärr: Förhindra division med noll ifall tickMultiplier blir 0.0
        const auto tickAt = ttq / juce::jmax(0.001f, currentTickMultiplier); 
        const auto tickLengthInSamples = (int) std::ceil (tickAt * bpSmp);

        const auto ppqFromBufStart = fmod (pos, tickAt);
        const double ppqOffset = tickAt - ppqFromBufStart;
        const auto bufStartInSecs = playheadPosition_.getTimeInSeconds().orFallback(0);
        const auto bufEndInSecs = bufStartInSecs + (buffer.getNumSamples() / sr);
        const double ppqEndValLocal = pos + ((bufEndInSecs - bufStartInSecs) * bps);
        const auto bufLengthInPPQ = bps * (buffer.getNumSamples() / sr);

        // stop if precount is on and counted enough bars
        handlePreCount (ppqEndValLocal + bufLengthInPPQ);

        auto ppqToBufEnd = bufLengthInPPQ;
        auto ppqPosInBuf = ppqOffset;
        auto currentSampleToTick = 0;

        // reset tick state
        tickState.tickStartPosition = 0;

        if (ppqFromBufStart == 0.0)
        {
            ppqPosInBuf = 0.0;
        }

        if (ticks.getNumOfTicks() == 0)
        {
            tickState.clear();
        }
        else
        {
            // FIX: Spela upp eventuellt pågående klick (svansen från förra blocket) INNAN vi letar efter nya klick.
            // Detta förhindrar "tidsresor" där framtida ljud läckte bakåt i bufferten vid snabba BPM!
            tickState.fillTickSample (buffer);

            while (ppqToBufEnd > ppqPosInBuf)
            {
                jassert (ppqToBufEnd >= ppqPosInBuf);
                // add tick(s) to current buffer
                currentSampleToTick = roundToInt (ppqPosInBuf * bpSmp);
                ppqPosInBuf += tickAt; // next sample
                int safeNumerator = juce::jmax(1, ts.numerator); // Säkerhetsspärr mot div-med-noll i fmod
                // FIX: Epsilon (+ 0.0001) förhindrar flyttals-missar, och + 1 garanterar att "Ettan" blir Beat 1 och inte Beat 4!
                tickState.beat = juce::roundToInt (floor (fmod ((pos + ppqPosInBuf + 0.0001) / ttq, safeNumerator))) + 1;
                const auto& beatAssign = settings.beatAssignments[jlimit (1, TickSettings::kMaxBeatAssignments, tickState.beat) - 1];
                const auto tickIdx = (size_t) jlimit (0, jmax ((int) ticks.getNumOfTicks() - 1, 0), beatAssign.tickIdx.get());
                tickState.refer[0] = ticks[tickIdx].getTickAudioBuffer();

                // 3. Kopiera ljudet säkert utan 'makeCopyOf' genom att använda den pre-allokerade bufferten
                // FIX: Begränsa alias-storleken mot faktiskt ALLOKERAT minne, annars riskerar 
                // vi Out-Of-Bounds minneskorruption om Sample Raten plötsligt förändras under drift!
                const int tickLen = jmin((int)ticks[tickIdx].getLengthInSamples(), tickState.sample.getNumSamples());
                
                // FIX: Om beatet är snabbare än ljudfilens längd, klipps ljudet av (truncation).
                // Genom att sätta 'playLen' fade:ar vi ut ljudet exakt vid klipp-punkten istället
                // för vid filens slut, vilket förhindrar digitala knäpp vid höga BPM!
                const int playLen = jmin(tickLengthInSamples, tickLen);
                
                if (playLen > 0)
                {
                    // DSP/CPU-Optimering: Använd ett AudioBuffer-alias ("vy") istället för setSize().
                    // Ett alias allokerar absolut noll minne, kan aldrig ta lås, och tvingar 
                    // effekterna att bara processa det faktiska ljudet istället för hela 10-sekundersbufferten.
                    juce::AudioBuffer<float> tickAlias (tickState.sample.getArrayOfWritePointers(), 1, playLen);
                    tickAlias.copyFrom(0, 0, tickState.refer[0], playLen);

                    if (tickState.currentSample >= 0)
                        TickUtils::fadeOut (tickAlias);
                    // hard-clip if needed
                    TickUtils::processClip (tickAlias);
                    
                    // Kombinera slagvolym och sample-volym för att appliceras effektivt vid mixning
                    tickState.beatGain = beatAssign.gain.get() * ticks[tickIdx].getGain();
                    // FIX: Begränsa läsningen till ljudfilens faktiska längd för att undvika spökecho/skräp-ljud!
                    tickState.addTickSample (buffer, currentSampleToTick, playLen);

                    // --- NY FUNKTION: Skicka MIDI-not ut för varje metronom-klick! ---
                    // Användarspecifika noter via inställningarna (Kanal 10 - Trummor)
                    const int midiNote = (tickState.beat == 1) ? currentNote1 : currentNoteOther;
                    const juce::uint8 velocity = (juce::uint8)juce::jlimit(1, 127, (int)(tickState.beatGain * 127.0f));
                    midiMessages.addEvent(juce::MidiMessage::noteOn(10, midiNote, velocity), currentSampleToTick);
                    
                    // FIX: Förhindra krasch (KERNELBASE.dll) om Cubase skickar "Flush"-buffertar med 0 samples (då blir index -1!)
                    const int noteOffSample = juce::jlimit(0, juce::jmax(0, buffer.getNumSamples() - 1), currentSampleToTick + 500);
                    midiMessages.addEvent(juce::MidiMessage::noteOff(10, midiNote, (juce::uint8)0), noteOffSample);
                }
            }
        }
        ticks.getLock().unlock();
        }
    }

    // 4. Filtrera ENDAST aktuellt block på mastern (t.ex. 256 samples istället för 10 sekunder)
    // Säkerhet: Vissa DAW:s skickar fler kanaler i 'buffer' än vad filtret är förberett på. Detta kraschar JUCE!
    const int numChannels = juce::jmin (buffer.getNumChannels(), (int)getTotalNumOutputChannels());
    // Säkerhet: Förhindra krasch vid flush-block (0 samples eller 0 kanaler)
    size_t safeChannels = (size_t) juce::jmax(0, numChannels);
    size_t safeSamples = (size_t) juce::jmax(0, buffer.getNumSamples());
    
    // FIX: Processa endast filtret om det faktiskt finns ljud att processa! Förhindrar Memory Access Violation.
    if (safeChannels > 0 && safeSamples > 0)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), safeChannels, safeSamples);
        juce::dsp::ProcessContextReplacing<float> context (block);
        lpfFilter.process (context);
    }

    // DSP-Optimering: Anropa inte 'std::pow' (decibelsToGain) för varje ljudblock!
    // Cacha multiplikatorn och uppdatera bara när volymen faktiskt ändras av användaren.
    if (lastMasterGainDb != currentMasterGain)
    {
        masterGainMultiplier = juce::Decibels::decibelsToGain (currentMasterGain);
        lastMasterGainDb = currentMasterGain;
    }
    buffer.applyGain (masterGainMultiplier);

    // LIVE SHOW OPTIMERING: Avancera standalone-tidslinjen HÄR i slutet av blocket!
    // Om vi gör det i början av processBlock hamnar MIDI-klockan och audioklicket 1 ljudblock i "framtiden",
    // vilket orsakar jitter och osynk. Nu räknas blocket ut exakt där vi är, sen flyttar vi fram markören.
    if (! isHostSyncSupported() || ! getState().useHostTransport.get())
    {
        if (playheadPosition_.getIsPlaying())
        {
            const double safeSr = juce::jmax(1.0, getSampleRate()); // Säkerhetsspärr mot div-med-noll
            const double bufInSecs = buffer.getNumSamples() / safeSr;
            const double iqps = playheadPosition_.getBpm().orFallback(120.0f) / 60.0;
            playheadPosition_.setPpqPosition (playheadPosition_.getPpqPosition().orFallback(0) + (iqps * bufInSecs));
            playheadPosition_.setTimeInSamples(playheadPosition_.getTimeInSamples().orFallback(0) + buffer.getNumSamples());
            playheadPosition_.setTimeInSeconds (playheadPosition_.getTimeInSeconds().orFallback(0) + bufInSecs);
        }
    }
    
    // Trådsäker överföring av UI-data (Undviker Undefined Behavior / Data Race i gränssnittet)
    const auto ppqForUI = playheadPosition_.getPpqPosition().orFallback(0);
    const auto tsForUI = playheadPosition_.getTimeSignature().orFallback(AudioPlayHead::TimeSignature ({1, 4}));
    const auto ttqForUI = (4.0 / juce::jmax(1, tsForUI.denominator)); 
    auto subDivForUI = fmod (ppqForUI, ttqForUI) / ttqForUI;
    uiCurrentBeatPos.store(tickState.beat + subDivForUI, std::memory_order_relaxed);
    uiCurrentBeat.store(tickState.beat, std::memory_order_relaxed);
}

//==============================================================================
bool TickAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* TickAudioProcessor::createEditor()
{
    return new TickAudioProcessorEditor (*this);
}

//==============================================================================
void TickAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // save
    MemoryOutputStream writeStream (destData, false);
    settings.cutoffFilter.setValue (filterCutoff->load(), nullptr);
    settings.masterGain.setValue (masterGain->load(), nullptr);
    settings.saveToArchive (writeStream, ticks, false, false);
}

void TickAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto* stream = new MemoryInputStream (data, (size_t) sizeInBytes, false);
    ZipFile archive (stream, true);
    settings.loadFromArchive (archive, ticks, false);
    auto* cutOff = parameters.getParameter (IDs::filterCutoff);
    cutOff->setValueNotifyingHost (cutOff->convertTo0to1 (settings.cutoffFilter.get()));
    auto* gain = parameters.getParameter (IDs::masterGain);
    gain->setValueNotifyingHost (gain->convertTo0to1 (settings.masterGain.get()));
}

double TickAudioProcessor::getCurrentBeatPos()
{
    // Blixtsnabb, 100% trådsäker avläsning för GUI
    return uiCurrentBeatPos.load(std::memory_order_relaxed);
}

void TickAudioProcessor::TickState::addTickSample (AudioBuffer<float>& bufferToFill, int startPos, int length)
{
    isClear = false;
    currentSample = 0;
    tickStartPosition = startPos;
    tickLengthInSamples = length;
    fillTickSample (bufferToFill);
    
    // FIX: Låt nästa ljudblock fortsätta spela upp klicket från början av sin buffer!
    // (Att sätta -1 här strypt/klippte av ljudet omedelbart efter första millisekunderna).
    tickStartPosition = 0;
}

void TickAudioProcessor::TickState::fillTickSample (AudioBuffer<float>& bufferToFill)
{
    if (tickStartPosition < 0)
        return; // fillTick was consumed

    if (currentSample < 0)
        return; // not active tick.

    auto constrainedLength = jmin (tickLengthInSamples - currentSample, sample.getNumSamples() - currentSample, bufferToFill.getNumSamples() - tickStartPosition);
    
    if (constrainedLength <= 0)
        return; // Säkerhetsspärr: Förhindra krasch om startpositionen ligger utanför bufferten

    const auto maxSampleChannelIndex = sample.getNumChannels() - 1;
    for (auto ch = 0; ch < bufferToFill.getNumChannels(); ch++)
    {
        // Använd addFrom för att mixa (inte skriva över) och applicera gain lokalt!
        bufferToFill.addFrom (ch, tickStartPosition, sample.getReadPointer(jlimit (0, maxSampleChannelIndex, ch), currentSample), constrainedLength, beatGain);
    }

    currentSample += constrainedLength;
    if (currentSample >= tickLengthInSamples || currentSample >= sample.getNumSamples())
    {
        currentSample = -1; // mark as not valid.
    }
}

void TickAudioProcessor::TickState::clear()
{
    isClear = true;
    currentSample = -1;
    tickLengthInSamples = tickStartPosition = beat = 0;
}

//==============================================================================
// TAP CC ÄNDRING: IMPLEMENTERING AV NY METOD
bool TickAudioProcessor::handleTapTempo(uint32_t nowMs)
{
    // defensiv debounce: förhindra snabb dubbel-anrop från host/MIDI/param-propagation
    constexpr uint32_t kTapDebounceMs = 150; // increased from 80 -> 150 ms

    const uint32_t prevMs = lastTapTimeMs.load(std::memory_order_relaxed);
    if (prevMs != 0 && (nowMs - prevMs) < kTapDebounceMs)
        return false;

    lastTapTimeMs.store(nowMs, std::memory_order_relaxed);

    double newBPM = ticks.tapModel.pushTap(nowMs);

    if (newBPM > 0.0)
    {
        uiUseHost.store(0, std::memory_order_relaxed);
        uiBpm.store((float)newBPM, std::memory_order_relaxed);
        uiIsPlaying.store(1, std::memory_order_relaxed);
        triggerAsyncUpdate();
        return true;
    }
    return false;
}

void TickAudioProcessor::handleAsyncUpdate()
{
    // Körs på Message-tråden – kraschar inte och pausar inte ljudet!
    if (tapTempoParam)
        tapTempoParam->setValueNotifyingHost (0.0f);
        
    // Töm den trådsäkra brevlådan till ValueTree säkert via UI-tråden!
    float b = uiBpm.exchange(-1.0f, std::memory_order_relaxed);
    if (b >= 0.0f) settings.transport.bpm.setValue(b, nullptr);
    
    int n = uiNumerator.exchange(-1, std::memory_order_relaxed);
    if (n >= 0) settings.transport.numerator.setValue(n, nullptr);
    
    int d = uiDenominator.exchange(-1, std::memory_order_relaxed);
    if (d >= 0) settings.transport.denumerator.setValue(d, nullptr);
    
    int p = uiIsPlaying.exchange(-1, std::memory_order_relaxed);
    if (p >= 0) settings.transport.isPlaying.setValue(p == 1, nullptr);
    
    int h = uiUseHost.exchange(-1, std::memory_order_relaxed);
    if (h >= 0) settings.useHostTransport.setValue(h == 1, nullptr);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TickAudioProcessor();
}