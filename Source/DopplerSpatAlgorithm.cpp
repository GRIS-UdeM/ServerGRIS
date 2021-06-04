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

#include "Constants.hpp"

static constexpr meters_t FIELD_RADIUS{ 50.0f };
static constexpr meters_t HEAD_RADIUS{ 0.075f };

static constexpr CartesianVector LEFT_EAR_POSITION{ -HEAD_RADIUS / FIELD_RADIUS, 0.0f, 0.0f };
static constexpr CartesianVector RIGHT_EAR_POSITION{ HEAD_RADIUS / FIELD_RADIUS, 0.0f, 0.0f };
static constexpr std::array<CartesianVector, 2> EARS_POSITIONS{ LEFT_EAR_POSITION, RIGHT_EAR_POSITION };

static constexpr CartesianVector UPPER_LEFT_CORNER{ 1.0f, 1.0f, 1.0f };

static auto const MAX_RELATIVE_DISTANCE{ (RIGHT_EAR_POSITION - UPPER_LEFT_CORNER).length() };
static auto const MAX_DISTANCE{ FIELD_RADIUS * MAX_RELATIVE_DISTANCE };

static constexpr auto SOUND_METERS_PER_SECOND = 400.0f;

static auto const SECONDS_TO_COVER_MAX_DISTANCE{ MAX_DISTANCE.get() / SOUND_METERS_PER_SECOND };

//==============================================================================
DopplerSpatAlgorithm::DopplerSpatAlgorithm(double const sampleRate, int const bufferSize)
{
    mData.bufferSize = bufferSize;
    mData.dopplerLength = std::ceil(SECONDS_TO_COVER_MAX_DISTANCE * sampleRate);
    auto const requiredSamples{ narrow<int>(mData.dopplerLength) + bufferSize };
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

    auto const numSamples{ sourcesBuffer.size() };

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

        for (int earIndex{}; earIndex < EARS_POSITIONS.size(); ++earIndex) {
            auto * buffer{ mData.dopplerLines.getWritePointer(earIndex) };

            auto const startingDistance{ sourceData.lastDistance };
            auto const targetDistance{ spatData[earIndex] };

            auto const distanceStep{ (targetDistance - startingDistance) / narrow<float>(numSamples) };

            auto const startingPosition{ mData.dopplerLength - startingDistance };
            auto const targetPosition{ mData.dopplerLength - targetDistance + narrow<float>(numSamples) };
        }
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
