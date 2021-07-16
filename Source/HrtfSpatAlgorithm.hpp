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

//==============================================================================
struct HrtfData {
    SpeakersAudioConfig speakersAudioConfig{};
    SpeakerAudioBuffer speakersBuffer{};
    StrongArray<output_patch_t, bool, MAX_NUM_SPEAKERS> hadSoundLastBlock{};
};

//==============================================================================
class HrtfSpatAlgorithm final : public AbstractSpatAlgorithm
{
    std::unique_ptr<AbstractSpatAlgorithm> mInnerAlgorithm{};
    HrtfData mHrtfData{};
    std::array<juce::dsp::Convolution, 16> mConvolutions{};

public:
    //==============================================================================
    HrtfSpatAlgorithm(SpeakerSetup const & speakerSetup,
                      SourcesData const & sources,
                      double sampleRate,
                      int bufferSize);
    //==============================================================================
    HrtfSpatAlgorithm() = delete;
    ~HrtfSpatAlgorithm() override = default;
    //==============================================================================
    HrtfSpatAlgorithm(HrtfSpatAlgorithm const &) = delete;
    HrtfSpatAlgorithm(HrtfSpatAlgorithm &&) = delete;
    HrtfSpatAlgorithm & operator=(HrtfSpatAlgorithm const &) = delete;
    HrtfSpatAlgorithm & operator=(HrtfSpatAlgorithm &&) = delete;
    //==============================================================================
    void updateSpatData(source_index_t sourceIndex, SourceData const & sourceData) noexcept override;
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourcesBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
                 juce::AudioBuffer<float> & stereoBuffer,
                 SourcePeaks const & sourcePeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override { return false; }
    [[nodiscard]] tl::optional<Error> getError() const noexcept override { return tl::nullopt; }
    //==============================================================================
    static std::unique_ptr<AbstractSpatAlgorithm>
        make(SpeakerSetup const & speakerSetup, SourcesData const & sources, double sampleRate, int bufferSize);

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(HrtfSpatAlgorithm)
};