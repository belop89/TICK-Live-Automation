#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class TapModel
{
public:
    // push tap and returns current detected BPM
    double pushTap (uint32_t nowMs);
    void clear();

    // Uppdaterad deklaration för trådsäker åtkomst
    double getLastDetectedBPM() const; 

    int getXforCurrentRange (uint32_t nowMs, int areaWidth) const;

    static double msToBPM (double ms);

private:
    // NYTT: Debounce tröskel (ms) för att ignorera dubbel-taps
    static constexpr int minTapIntervalMs = 80;   
    // NYTT: Starta om från noll om det går över 3 sekunder mellan trycken
    static constexpr int maxTapIntervalMs = 3000; 
    static constexpr auto maxPointsToKeep = 5; // 5 punkter ger 4 intervaller för ett stabilt rullande medelvärde
    
    // 100% Lock-Free DSP-Optimering
    std::array<std::atomic<uint32_t>, maxPointsToKeep> tapPoints {};
    std::atomic<int> numTapPoints { 0 };
    
    // DSP-Optimering: Trådsäker läsning utan SpinLock-risk från GUI
    std::atomic<double> lastDetectedBPM { 0.0 };
};