/*
 This file is part of SpatGRIS.

 Developers: Samuel B�land, Olivier B�langer, Nicolas Masson

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

//==============================================================================
DopplerSpatAlgorithm::DopplerSpatAlgorithm(double const sampleRate, int const bufferSize)
{
    mData.sampleRate = sampleRate;
    auto const dopplerLength{ MAX_DISTANCE.get() / SOUND_METERS_PER_SECOND * sampleRate };
    auto const requiredSamples{ narrow<int>(std::round(dopplerLength)) + bufferSize };
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

    auto const findOffender = [](float const * begin, float const * end) {
        auto const minMax{ std::minmax(begin, end) };
        auto const max{ std::max(std::abs(*minMax.first), std::abs(*minMax.second)) };

        if (max > 1.0f) {
            auto const * offender{ std::abs(*minMax.first) > 1.0f ? minMax.first : minMax.second };
            auto const distance{ offender - begin };
            int lol{};
        }
    };

    // speakersBuffer.silence();
    auto const bufferSize{ sourcesBuffer.getNumSamples() };
    auto const dopplerBufferSize{ mData.dopplerLines.getNumSamples() };

    for (auto const & source : config.sourcesAudioConfig) {
        if (sourcePeaks[source.key] < SMALL_GAIN || source.value.directOut || source.value.isMuted) {
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

        findOffender(sourceSamples, sourceSamples + bufferSize);

        for (size_t earIndex{}; earIndex < EARS_POSITIONS.size(); ++earIndex) {
            auto * dopplerSamples{ mData.dopplerLines.getWritePointer(narrow<int>(earIndex)) };

            findOffender(dopplerSamples, dopplerSamples + dopplerBufferSize);

            auto const MAX_DISTANCE_DIFF = meters_t{ 2.0f };

            auto const beginAbsoluteDistance{ FIELD_RADIUS * lastSpatData[earIndex] };
            auto const endAbsoluteDistance{ std::clamp(FIELD_RADIUS * spatData[earIndex],
                                                       beginAbsoluteDistance - MAX_DISTANCE_DIFF,
                                                       beginAbsoluteDistance + MAX_DISTANCE_DIFF) };

            auto const beginDopplerIndex{ narrow<int>(
                std::round(beginAbsoluteDistance.get() * mData.sampleRate / SOUND_METERS_PER_SECOND)) };
            auto const endDopplerIndex{ narrow<int>(std::round(endAbsoluteDistance.get() * mData.sampleRate
                                                               / SOUND_METERS_PER_SECOND))
                                        + bufferSize };
            auto numOutSamples{ endDopplerIndex - beginDopplerIndex };

            auto const reverse{ numOutSamples < 0 };

            if (reverse) {
                numOutSamples = -numOutSamples;
                std::reverse(sourceSamples, sourceSamples + bufferSize);
            }

            auto * startingSample{ dopplerSamples + beginDopplerIndex };

            auto const sampleRatio{ narrow<double>(bufferSize) / narrow<double>(numOutSamples) };

            auto & interpolator{ mInterpolators[source.key][earIndex] };
            interpolator.processAdding(sampleRatio, sourceSamples, startingSample, numOutSamples, 1.0f);

            findOffender(startingSample, startingSample + bufferSize);

            if (reverse) {
                std::reverse(sourceSamples, sourceSamples + bufferSize);
            }

            lastSpatData[earIndex] = endAbsoluteDistance / FIELD_RADIUS;
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

        findOffender(dopplerSamplesBegin, dopplerSamplesEnd);
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
