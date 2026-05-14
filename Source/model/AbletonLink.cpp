#include "AbletonLink.h"

AbletonLink::AbletonLink()
{
    // Skapa en Link-instans med start-tempo 120 BPM
    link = std::make_unique<ableton::Link> (120.0);
    link->enable (true); // Aktivera nätverkslyssning direkt!
}

AbletonLink::~AbletonLink()
{
    if (link)
        link->enable (false);
}

bool AbletonLink::isLinkConnected() const
{
    // SÄKERHET: Vi tar bort kravet på 'numPeers > 0'. Detta gör att TICK alltid 
    // underhåller och uppdaterar Link-sessionen i bakgrunden. Om TouchDesigner 
    // startas eller ansluter senare, tvingas TD direkt in i TICKs aktuella tempo!
    return link && link->isEnabled();
}

void AbletonLink::setEnabled (bool shouldBeEnabled)
{
    if (link)
        link->enable (shouldBeEnabled);
}

bool AbletonLink::isEnabled() const
{
    return link && link->isEnabled();
}

void AbletonLink::linkPosition (juce::AudioPlayHead::PositionInfo& posInfo, const Requests& requests)
{
    if (! link || ! link->isEnabled())
        return;

    auto sessionState = link->captureAppSessionState();
    const auto time = link->clock().micros();
    bool stateChanged = false;

    // Fas-lås TouchDesigner och resten av nätverket till vårt aktuella slag
    const auto ts = posInfo.getTimeSignature().orFallback (juce::AudioPlayHead::TimeSignature {4, 4});
    
    // Nätverks-Fix: Ableton Link använder alltid "Fjärdedelsnoter" som basenhet för sin Phase-loop! 
    // Vi måste konvertera DAW:ens taktart (ex 6/8 eller 7/8) till exakta fjärdedelsnoter, annars roterar TouchDesigner i fel hastighet!
    const double quantum = juce::jmax (1.0, (4.0 / juce::jmax(1, ts.denominator)) * ts.numerator);

    if (requests.forceBeatAtTime.has_value())
    {
        sessionState.requestBeatAtTime (*requests.forceBeatAtTime, time, quantum);
        stateChanged = true;
    }

    // Applicera ändringar från pluginen (ex. Tap Tempo från Helix) till nätverket
    if (requests.bpm.has_value())
    {
        if (std::abs(sessionState.tempo() - *requests.bpm) > 0.001)
        {
            sessionState.setTempo (*requests.bpm, time);
            stateChanged = true;
        }
    }
        
    if (requests.isPlaying.has_value())
    {
        if (sessionState.isPlaying() != *requests.isPlaying)
        {
            sessionState.setIsPlaying (*requests.isPlaying, time);
            stateChanged = true;
        }
    }

    if (stateChanged)
        link->commitAppSessionState (sessionState);

    // Hämta tillbaka den synkade nätverkstiden för metronom-klicket
    posInfo.setBpm (sessionState.tempo());
    posInfo.setIsPlaying (sessionState.isPlaying());
    
    // SÄKERHET (Phase-lock): Lås även ljudklickets "Etta" (Bar Start) till 
    // Link-nätverkets matematiska fas för att förhindra drift mot lasern i TD!
    const double beat = sessionState.beatAtTime (time, quantum);
    posInfo.setPpqPosition (beat);
    posInfo.setPpqPositionOfLastBarStart (beat - sessionState.phaseAtTime(time, quantum));
}