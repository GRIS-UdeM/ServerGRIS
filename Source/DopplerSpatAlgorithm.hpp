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

#pragma once

#include "AbstractSpatAlgorithm.hpp"

using DopplerSpatData = std::array<float, 2>;
using DopplerSpatDataQueue = AtomicExchanger<DopplerSpatData>;

struct DopplerSourceData {
    DopplerSpatDataQueue spatDataQueue{};
    DopplerSpatDataQueue::Ticket * mostRecentSpatData{};
    float lastDistance{};
};

struct DopplerData {
    StrongArray<source_index_t, DopplerSourceData, MAX_NUM_SOURCES> sourcesData{};
    juce::AudioBuffer<float> dopplerLines{};
    float dopplerLength{};
    int bufferSize{};
};

//==============================================================================
class DopplerSpatAlgorithm final : public AbstractSpatAlgorithm
{
    DopplerData mData{};

public:
    //==============================================================================
    DopplerSpatAlgorithm(double sampleRate, int bufferSize);
    ~DopplerSpatAlgorithm() = default;
    //==============================================================================
    DopplerSpatAlgorithm(DopplerSpatAlgorithm const &) = delete;
    DopplerSpatAlgorithm(DopplerSpatAlgorithm &&) = delete;
    DopplerSpatAlgorithm & operator=(DopplerSpatAlgorithm const &) = delete;
    DopplerSpatAlgorithm & operator=(DopplerSpatAlgorithm &&) = delete;
    //==============================================================================
    void updateSpatData(source_index_t sourceIndex, SourceData const & sourceData) noexcept override;
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourcesBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
                 SourcePeaks const & sourcePeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(DopplerSpatAlgorithm)
};