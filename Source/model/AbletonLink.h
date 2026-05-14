#pragma once

#include <JuceHeader.h>
#include <ableton/Link.hpp>
#include <optional>
#include <memory>

class AbletonLink
{
public:
    struct Requests
    {
        std::optional<bool> isPlaying;
        std::optional<double> bpm;
        std::optional<double> forceBeatAtTime;
    };

    AbletonLink();
    ~AbletonLink();

    bool isLinkConnected() const;
    void linkPosition (juce::AudioPlayHead::PositionInfo& posInfo, const Requests& requests);
    
    // Möjliggör av/på-slag av nätverkssynken
    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const;

private:
    std::unique_ptr<ableton::Link> link;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbletonLink)
};