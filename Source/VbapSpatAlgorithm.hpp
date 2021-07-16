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
#include "StaticMap.hpp"
#include "StrongArray.hpp"
#include "TaggedAudioBuffer.hpp"
#include "vbap.hpp"

VbapType getVbapType(SpeakersData const & speakers);

struct VbapSourceData {
    AtomicExchanger<SpeakersSpatGains> spatDataQueue{};
    AtomicExchanger<SpeakersSpatGains>::Ticket * currentSpatData{};
    SpeakersSpatGains lastGains{};
};

using VbapSourcesData = StrongArray<source_index_t, VbapSourceData, MAX_NUM_SOURCES>;

//==============================================================================
class VbapSpatAlgorithm final : public AbstractSpatAlgorithm
{
    std::unique_ptr<VbapData> mSetupData{};
    VbapSourcesData mData{};

public:
    //==============================================================================
    explicit VbapSpatAlgorithm(SpeakersData const & speakers);
    ~VbapSpatAlgorithm() override = default;
    //==============================================================================
    VbapSpatAlgorithm(VbapSpatAlgorithm const &) = delete;
    VbapSpatAlgorithm(VbapSpatAlgorithm &&) = delete;
    VbapSpatAlgorithm & operator=(VbapSpatAlgorithm const &) = delete;
    VbapSpatAlgorithm & operator=(VbapSpatAlgorithm &&) = delete;
    //==============================================================================
    void updateSpatData(source_index_t sourceIndex, SourceData const & sourceData) noexcept override;
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourcesBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
                 juce::AudioBuffer<float> & stereoBuffer,
                 SourcePeaks const & sourcePeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override;
    [[nodiscard]] tl::optional<Error> getError() const noexcept override { return tl::nullopt; }
    //==============================================================================
    static std::unique_ptr<AbstractSpatAlgorithm> make(SpeakerSetup const & speakerSetup);

private:
    JUCE_LEAK_DETECTOR(VbapSpatAlgorithm)
};
