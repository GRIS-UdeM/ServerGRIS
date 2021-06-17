/*
 This file is part of SpatGRIS.

 Developers: Samuel Béland, Olivier Bélanger, Nicolas Masson

 SpatGRIS is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 SpatGRIS is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with SpatGRIS.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "DopplerSpatAlgorithm.hpp"

static void interpolate(float const * inputSamples,
                        int const numInputSamples,
                        float * outputSamples,
                        int const numOutputSamples) noexcept
{
#ifndef NDEBUG
    auto const * const inEnd{ inputSamples + numInputSamples };
#endif

    if (numInputSamples == numOutputSamples) {
        std::copy_n(inputSamples, numInputSamples, outputSamples);
        return;
    }

    auto const * const outEnd{ outputSamples + numOutputSamples };
    auto const inputPerOutput{ static_cast<double>(numInputSamples - 1) / static_cast<double>(numOutputSamples) };
    auto subSamplePos{ inputPerOutput };
    auto lastSample{ *inputSamples++ };

    while (outputSamples < outEnd) {
        while (subSamplePos >= 1.0) {
            jassert(inputSamples < inEnd);
            lastSample = *inputSamples++;
            subSamplePos -= 1.0;
        }

        *outputSamples++ = static_cast<float>(*inputSamples * subSamplePos + lastSample * (1.0 - subSamplePos));
        subSamplePos += inputPerOutput;
    }
}

//==============================================================================
DopplerSpatAlgorithm::DopplerSpatAlgorithm(double const sampleRate, int const bufferSize)
{
    mData.sampleRate = sampleRate;
    auto const dopplerLength{ MAX_DISTANCE.get() / SOUND_METERS_PER_SECOND * sampleRate };
    auto const requiredSamples{ narrow<int>(std::ceil(dopplerLength)) + bufferSize };
    mData.dopplerLines.setSize(2, requiredSamples);
    mData.dopplerLines.clear();
}

//==============================================================================
void DopplerSpatAlgorithm::updateSpatData(source_index_t const sourceIndex, SourceData const & sourceData) noexcept
{
    jassert(sourceData.position);

    auto & sourceSpatData{ mData.sourcesData[sourceIndex] };
    auto & exchanger{ sourceSpatData.spatDataQueue };

    auto * ticket{ exchanger.acquire() };

    auto & spatData{ ticket->get() };

    auto const & sourcePosition{ *sourceData.position };
    auto const leftEarDistance{ (LEFT_EAR_POSITION - sourcePosition).length() / MAX_RELATIVE_DISTANCE };
    auto const rightEarDistance{ (RIGHT_EAR_POSITION - sourcePosition).length() / MAX_RELATIVE_DISTANCE };

    spatData[0] = leftEarDistance;
    spatData[1] = rightEarDistance;

    exchanger.setMostRecent(ticket);
}

//==============================================================================
void DopplerSpatAlgorithm::process(AudioConfig const & config,
                                   SourceAudioBuffer & sourcesBuffer,
                                   SpeakerAudioBuffer & speakersBuffer,
                                   SourcePeaks const & sourcePeaks,
                                   [[maybe_unused]] SpeakersAudioConfig const * altSpeakerConfig)
{
    ASSERT_AUDIO_THREAD;
    jassert(!altSpeakerConfig);

    auto const bufferSize{ sourcesBuffer.getNumSamples() };
    auto const dopplerBufferSize{ mData.dopplerLines.getNumSamples() };

    for (auto const & source : config.sourcesAudioConfig) {
        if (source.value.directOut || source.value.isMuted) {
            continue;
        }

        auto & sourceData{ mData.sourcesData[source.key] };
        auto *& ticket{ sourceData.mostRecentSpatData };
        sourceData.spatDataQueue.getMostRecent(ticket);

        if (ticket == nullptr) {
            continue;
        }

        auto const & spatData{ ticket->get() };
        auto & lastSpatData{ mData.lastSpatData[source.key] };
        auto * sourceSamples{ sourcesBuffer[source.key].getWritePointer(0) };

        for (size_t earIndex{}; earIndex < EARS_POSITIONS.size(); ++earIndex) {
            auto * dopplerSamples{ mData.dopplerLines.getWritePointer(narrow<int>(earIndex)) };

            auto const MAX_DISTANCE_DIFF = meters_t{ 1000.0f };

            auto const beginAbsoluteDistance{ FIELD_RADIUS * lastSpatData[earIndex] };
            auto const endAbsoluteDistance{ std::clamp(FIELD_RADIUS * spatData[earIndex],
                                                       beginAbsoluteDistance - MAX_DISTANCE_DIFF,
                                                       beginAbsoluteDistance + MAX_DISTANCE_DIFF) };

            auto const beginDopplerIndex{ narrow<int>(
                std::floor(beginAbsoluteDistance.get() * mData.sampleRate / SOUND_METERS_PER_SECOND)) };
            auto const endDopplerIndex{ narrow<int>(std::floor(endAbsoluteDistance.get() * mData.sampleRate
                                                               / SOUND_METERS_PER_SECOND))
                                        + bufferSize };

            auto numOutSamples{ endDopplerIndex - beginDopplerIndex };

            auto const reverse{ numOutSamples < 1 };

            if (reverse) {
                std::reverse(sourceSamples, sourceSamples + bufferSize);
                numOutSamples *= -1;
            }

            auto * startingDopplerSample{ dopplerSamples + beginDopplerIndex };
            if (numOutSamples != 0) {
                interpolate(sourceSamples, bufferSize, startingDopplerSample, numOutSamples);
            }

            if (reverse) {
                std::reverse(sourceSamples, sourceSamples + bufferSize);
            }

            lastSpatData[earIndex] = endAbsoluteDistance / FIELD_RADIUS;

            static constexpr auto POW{ 1.5f };
            auto const startGain{ std::min(1.0f / std::pow(beginAbsoluteDistance.get(), POW), 1.0f) };
            auto const endGain{ std::min(1.0f / std::pow(endAbsoluteDistance.get(), POW), 1.0f) };

            juce::AudioBuffer<float>{ &startingDopplerSample, 1, numOutSamples }.applyGainRamp(0,
                                                                                               0,
                                                                                               numOutSamples,
                                                                                               startGain,
                                                                                               endGain);
        }
    }

    auto speakerIt{ speakersBuffer.begin() };
    for (int channel{}; channel < mData.dopplerLines.getNumChannels(); ++channel) {
        auto * dopplerSamplesBegin{ mData.dopplerLines.getWritePointer(channel) };
        auto * dopplerSamplesEnd{ dopplerSamplesBegin + dopplerBufferSize };
        auto * speakerSamples{ (speakerIt++)->value->getWritePointer(0) };

        std::copy_n(dopplerSamplesBegin, bufferSize, speakerSamples);
        std::fill_n(dopplerSamplesBegin, bufferSize, 0.0f);
        std::rotate(dopplerSamplesBegin, dopplerSamplesBegin + bufferSize, dopplerSamplesEnd);
    }
}

//==============================================================================
juce::Array<Triplet> DopplerSpatAlgorithm::getTriplets() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassertfalse;
    return juce::Array<Triplet>{};
}

//==============================================================================
bool DopplerSpatAlgorithm::hasTriplets() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return false;
}
