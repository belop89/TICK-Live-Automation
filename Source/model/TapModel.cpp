#include "TapModel.h"
#include <cmath>

double TapModel::pushTap (uint32_t nowMs)
{
    // HELT LOCK-FREE: Inga trådlås används, vilket eliminerar Priority Inversion-risken!
    int currentNum = numTapPoints.load(std::memory_order_relaxed);

    // Debounce: om senaste tap är för nära i tiden, ignorera
    if (currentNum > 0)
    {
        const auto deltaMs = nowMs - tapPoints[0].load(std::memory_order_relaxed);
        
        // Säkerhets-debounce: ignorera om det gått för kort tid
        if (deltaMs < minTapIntervalMs)
            return lastDetectedBPM.load(std::memory_order_relaxed);
            
        // Återställ listan om det gått för lång tid (över 3 sekunder) - ny låt/nytt tempo!
        if (deltaMs > maxTapIntervalMs)
        {
            currentNum = 0;
            // Nollställ referensen, så att nästa låt kan börja på ett helt nytt (betydligt snabbare/långsammare) tempo!
            lastDetectedBPM.store(0.0, std::memory_order_relaxed);
        }
    }

    // Skifta elementen nedåt för att ge plats i början (index 0) utan allokering
    const int shiftCount = (currentNum < maxPointsToKeep) ? currentNum : maxPointsToKeep - 1;
    for (int i = shiftCount; i > 0; --i)
        tapPoints[i].store(tapPoints[i - 1].load(std::memory_order_relaxed), std::memory_order_relaxed);
    
    tapPoints[0].store(nowMs, std::memory_order_relaxed);
    if (currentNum < maxPointsToKeep)
        currentNum++;

    numTapPoints.store(currentNum, std::memory_order_relaxed);

    // Tre-klicks-regeln: Kräver minst 3 klick för att sätta tempo (skydd mot Helix spök-klick)
    if (currentNum < 3)
        return lastDetectedBPM.load(std::memory_order_relaxed);
        
    // Matte-optimering: Beräkna snitt direkt mellan första och sista punkten
    const double avgDelta = (double)(tapPoints[0].load(std::memory_order_relaxed) - tapPoints[currentNum - 1].load(std::memory_order_relaxed)) / (currentNum - 1);
    const double newBPM = msToBPM (avgDelta);

    // Helix-skyddet: Om tempot plötsligt diffar med mer än 50 BPM från vårt nuvarande tempo,
    // är det förmodligen Helix "spök-slag" (hårdvaru-quirk). Vi förkastar det och skyddar takten!
    constexpr auto bpmDriftTolerance = 50.0;
    const double currentBpm = lastDetectedBPM.load(std::memory_order_relaxed);
    if (currentBpm > 0.0 && std::abs (newBPM - currentBpm) > bpmDriftTolerance)
    {
        tapPoints[0].store(nowMs, std::memory_order_relaxed);
        numTapPoints.store(1, std::memory_order_relaxed);
        return currentBpm;
    }

    lastDetectedBPM.store(newBPM, std::memory_order_relaxed);
    return newBPM;
}

int TapModel::getXforCurrentRange (uint32_t nowMs, int areaWidth) const
{
    int currentNum = numTapPoints.load(std::memory_order_relaxed);

    if (currentNum == 0)
        return 0;

    const auto start = tapPoints[currentNum - 1].load(std::memory_order_relaxed);
    const float range = (float)(tapPoints[0].load(std::memory_order_relaxed) - start);
    
    // Säker och snabb hantering av 0-range för att undvika division-med-noll
    if (areaWidth <= 1 || range <= 0.0f)
        return 0;

    const auto pos = nowMs - start;

    const auto x = juce::roundToInt ((pos / range) * (areaWidth - 1));
    return juce::jlimit (0, areaWidth - 1, x);
}

void TapModel::clear()
{
    numTapPoints.store(0, std::memory_order_relaxed);
    lastDetectedBPM.store(0.0, std::memory_order_relaxed);
}

double TapModel::getLastDetectedBPM() const
{
    // Trådsäker och blixtsnabb "lock-free" avläsning tack vare atomics
    return lastDetectedBPM.load(std::memory_order_relaxed);
}

double TapModel::msToBPM (double ms)
{
    if (ms <= 0.0) return 0.0;
    auto secs = ms / 1000.0;
    return 60.0 / secs;
}
