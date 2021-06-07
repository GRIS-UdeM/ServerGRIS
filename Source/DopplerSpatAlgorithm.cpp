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

//==============================================================================
DopplerSpatAlgorithm::DopplerSpatAlgorithm(double const sampleRate, int const bufferSize)
{
    auto const dopplerLength{ MAX_DISTANCE.get() / SOUND_METERS_PER_SECOND * sampleRate };
    auto const requiredSamples{ narrow<int>(std::ceil(dopplerLength)) + bufferSize };
    mData.dopplerLines.setSize(2, requiredSamples);
}

//==============================================================================
void DopplerSpatAlgorithm::updateSpatData(source_index_t const sourceIndex, SourceData const & sourceData) noexcept
{
    jassert(sourceData.position);

    auto & sourceSpatData{ mData.sourcesData[sourceIndex] };
    auto & exchanger{ sourceSpatData.spatDataQueue };
    auto * ticket{ sourceSpatData.mostRecentSpatData };
    exchanger.getMostRecent(ticket);

    auto & spatData{ ticket->get() };

    auto const & sourcePosition{ *sourceData.position };
    auto const leftEarDistance{ (LEFT_EAR_POSITION - sourcePosition).length() };
    auto const rightEarDistance{ (RIGHT_EAR_POSITION - sourcePosition).length() };

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

    auto const bufferSize{ sourcesBuffer.size() };
    auto const dopplerBufferSize{ mData.dopplerLines.getNumSamples() };

    for (auto const & source : config.sourcesAudioConfig) {
        if (source.value.directOut || source.value.isMuted) {
            continue;
        }

        auto & sourceData{ mData.sourcesData[source.key] };
        auto * ticket{ sourceData.mostRecentSpatData };
        sourceData.spatDataQueue.getMostRecent(ticket);

        if (ticket == nullptr) {
            continue;
        }

        auto const & spatData{ ticket->get() };
        auto const * inBuffer{ sourcesBuffer[source.key].getReadPointer(0) };

        for (size_t earIndex{}; earIndex < EARS_POSITIONS.size(); ++earIndex) {
            auto * outBuffer{ mData.dopplerLines.getWritePointer(narrow<int>(earIndex)) };

            auto const startingDistance{ MAX_DISTANCE * sourceData.lastDistance };
            auto const targetDistance{ MAX_DISTANCE * spatData[earIndex] };

            auto const startingSampleIndex{ dopplerBufferSize - bufferSize
                                            - narrow<int>(std::floor(startingDistance.get() * mData.sampleRate
                                                                     / SOUND_METERS_PER_SECOND)) };
            auto const targetSampleIndex{ dopplerBufferSize
                                          - narrow<int>(std::ceil(targetDistance.get() * mData.sampleRate
                                                                  / SOUND_METERS_PER_SECOND)) };

            auto * startingSample{ outBuffer + startingSampleIndex };

            auto const numOutSamples{ targetSampleIndex - startingSampleIndex };

            auto const sampleRatio{ narrow<double>(numOutSamples) / narrow<double>(bufferSize) };

            auto & interpolator{ mInterpolators[earIndex] };
            interpolator.processAdding(sampleRatio, inBuffer, startingSample, numOutSamples, 1.0f);

            sourceData.lastDistance = spatData[earIndex];
        }
    }

    auto speakerIt{ speakersBuffer.begin() };
    auto const offset{ dopplerBufferSize - bufferSize };
    for (int channel{}; channel < mData.dopplerLines.getNumChannels(); ++channel) {
        auto * dopplerBegin{ mData.dopplerLines.getWritePointer(channel) };
        auto * dopplerEnd{ mData.dopplerLines.getWritePointer(channel) + dopplerBufferSize };
        auto const * in{ mData.dopplerLines.getReadPointer(channel) + offset };
        auto * out{ speakerIt++->value->getWritePointer(0) };
        std::memcpy(out, in, bufferSize);
        std::rotate(dopplerBegin, dopplerBegin + bufferSize, dopplerEnd);
        std::fill_n(dopplerBegin, bufferSize, 0.0f);
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
