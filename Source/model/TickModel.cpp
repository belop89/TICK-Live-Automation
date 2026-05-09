/*
  ==============================================================================

    TickModel.cpp
    Created: 13 Sep 2019 9:36:39am
    Author:  Tal Aviram

  ==============================================================================
*/

#include "TickModel.h"

#include "../libsamplerate/src/samplerate.h"
#include <cassert>
#include <cmath>
#include <cstring>

static const auto maxSamples = 2.0 * BASE_SAMPLERATE;

Tick::Tick (const char* tickName, const float* const* audioToSave, const int numOfSamples, const int numOfChannels, double sampleRate) : name (tickName),
                                                                                                                                         lastSampleRate (BASE_SAMPLERATE),
                                                                                                                                         resampled ({}),
                                                                                                                                         numOfResampledSamples (0),
                                                                                                                                         startPosInSec (0),
                                                                                                                                         endPosInSec (numOfSamples / BASE_SAMPLERATE)
{
    // limit to sample up to 2 seconds.
    const auto samplesToRead = std::min<double> (numOfSamples, 2.0 * sampleRate);
    const auto resampleRatio = BASE_SAMPLERATE / sampleRate;
    numOfTickSamples = static_cast<size_t> (std::min (std::round (samplesToRead * resampleRatio), maxSamples));

    sample = std::make_unique<float[]> (numOfTickSamples); // this also zeros out samples.
    auto summing_buffer = std::make_unique<float[]> (numOfTickSamples);
    const auto gainFactor = 1.0f / std::max(1, numOfChannels);

    for (auto ch = 0; ch < numOfChannels; ch++)
    {
        // needs resample?
        if (resampleRatio != 1.0)
        {
            mono_resample (audioToSave[ch], summing_buffer.get(), resampleRatio, static_cast<size_t> (numOfSamples), numOfTickSamples);
        }
        else
        {
            memcpy (summing_buffer.get(), audioToSave[ch], numOfTickSamples * sizeof (float));
        }

        // SIMD-Optimering: Använd processorns hårdvaru-vektorisering för blixtsnabb summering av kanaler
        juce::FloatVectorOperations::addWithMultiply(sample.get(), summing_buffer.get(), gainFactor, (int)numOfTickSamples);
    }
    
    updatePrecalculatedSamples();
}

std::string Tick::getName() const
{
    return name;
}

void Tick::mono_resample (const float* src, float* dst, double ratio, const size_t numOfInSamples, const size_t numOfOutSamples)
{
    SRC_DATA src_data;
    src_data.data_in = src;
    src_data.input_frames = static_cast<long> (numOfInSamples);
    src_data.src_ratio = ratio;
    src_data.output_frames = static_cast<long> (numOfOutSamples);
    src_data.data_out = dst;
    // resample single channel
    [[maybe_unused]] const auto res = src_simple (&src_data, SRC_SINC_BEST_QUALITY, 1);
    assert (res == 0);
}

void Tick::updatePrecalculatedSamples()
{
    startPosInSamples = static_cast<int> (std::round (getSampleRate() * startPosInSec));

    auto numSamples = resampled ? numOfResampledSamples : getSourceLengthInSamples();
    if (endPosInSec > 0)
        lengthInSamples = std::min<int> (static_cast<int> (std::round ((endPosInSec - startPosInSec) * getSampleRate())), numSamples);
    else
        lengthInSamples = numSamples;
}

int Tick::getLengthInSamples() const
{
    return lengthInSamples;
}

int Tick::getSourceLengthInSamples() const
{
    return static_cast<int> (numOfTickSamples);
}

double Tick::getSourceLengthInSec() const
{
    return getSourceLengthInSamples() / BASE_SAMPLERATE;
}

void Tick::setSampleRate (double newSampleRate)
{
    lastSampleRate = newSampleRate;
    const auto newRatio = newSampleRate / BASE_SAMPLERATE;
    numOfResampledSamples = 0;
    resampled.reset();

    if (newRatio == 1.0)
    {
        updatePrecalculatedSamples();
        return;
    }

    numOfResampledSamples = static_cast<int> (std::round (numOfTickSamples * newRatio));
    resampled = std::make_unique<float[]> (static_cast<size_t> (numOfResampledSamples));
    mono_resample (sample.get(), resampled.get(), newRatio, static_cast<size_t> (numOfTickSamples), static_cast<size_t> (numOfResampledSamples));
    
    updatePrecalculatedSamples();
}

double Tick::getSampleRate() const
{
    return lastSampleRate;
}

float* Tick::getTickAudioBuffer()
{
    if (resampled)
        return &resampled.get()[startPosInSamples];
    else
        return &getTickSource()[startPosInSamples];
}

float* Tick::getTickSource()
{
    return sample.get();
}

double Tick::getStartInSec() const
{
    return startPosInSec;
}

double Tick::getEndInSec() const
{
    return endPosInSec;
}

void Tick::setRange (double startInSec, double endInSec)
{
    assert (startInSec >= 0 && std::round (endInSec * BASE_SAMPLERATE) <= numOfTickSamples);
    startPosInSec = startInSec;
    endPosInSec = endInSec;
    updatePrecalculatedSamples();
}

float Tick::getGain() const
{
    return gain;
}

void Tick::setGain (float newGain)
{
    gain = newGain;
}

void Ticks::addTick (std::unique_ptr<Tick>&& tickToAdd)
{
    const scoped_lock sl (inuseLock);
    ticks.push_back (std::move (tickToAdd));
}

void Ticks::replaceTick (const int idx, std::unique_ptr<Tick>&& newTick)
{
    const scoped_lock sl (inuseLock);
    ticks[static_cast<size_t> (idx)] = std::move (newTick);
}

void Ticks::removeTick (int idx)
{
    const scoped_lock sl (inuseLock);
    assert (idx >= 0 && idx < static_cast<int> (ticks.size()));
    ticks.erase (ticks.begin() + idx);
}

void Ticks::clear()
{
    const scoped_lock sl (inuseLock);
    ticks.clear();
}

void Ticks::addTicks (std::vector<std::unique_ptr<Tick>> ticksToAdd, const double samplerate, const bool clearTicks)
{
    const scoped_lock sl (inuseLock);
    if (clearTicks)
        ticks.clear();

    for (auto& tick : ticksToAdd)
    {
        tick->setSampleRate (samplerate > 0 ? samplerate : BASE_SAMPLERATE);
        ticks.push_back (std::move (tick));
    }
    ticksToAdd.clear();
}

size_t Ticks::getNumOfTicks() const
{
    return ticks.size();
}
