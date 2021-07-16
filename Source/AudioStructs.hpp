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

#include "AtomicExchanger.hpp"
#include "SpatMode.hpp"
#include "StaticMap.hpp"
#include "StrongArray.hpp"
#include "constants.hpp"

enum class VbapType { twoD, threeD };

float constexpr SMALL_GAIN = 0.0000000000001f;

//==============================================================================
struct SpeakerHighpassState {
    double x1{};
    double x2{};
    double x3{};
    double x4{};
    double y1{};
    double y2{};
    double y3{};
    double y4{};
};

//==============================================================================
struct SpeakerHighpassConfig {
    double b1{};
    double b2{};
    double b3{};
    double b4{};
    double ha0{};
    double ha1{};
    double ha2{};
    //==============================================================================
    void process(float * data, int numSamples, SpeakerHighpassState & state) const;
};

//==============================================================================
struct SpeakerAudioConfig {
    bool isDirectOutOnly{};
    float gain{};
    bool isMuted{};
    tl::optional<SpeakerHighpassConfig> highpassConfig{};
};

//==============================================================================
struct SpeakerAudioState {
    SpeakerHighpassState highpassState{};
};

//==============================================================================
struct LbapSourceAttenuationState {
    float currentGain{};
    float currentCoefficient{};
    float lowpassY{};
    float lowpassZ{};
};

//==============================================================================
struct LbapAttenuationConfig {
    float linearGain{};
    float lowpassCoefficient{};
    //==============================================================================
    void process(float * data, int numSamples, float distance, LbapSourceAttenuationState & state) const;
};

//==============================================================================
using SpeakersSpatGains = StrongArray<output_patch_t, float, MAX_NUM_SPEAKERS>;

//==============================================================================
struct SourceAudioState {
    SpeakersSpatGains lastSpatGains{};

    // LBAP-specific
    LbapSourceAttenuationState lbapAttenuationState{};
    // STEREO-specific
    radians_t stereoLastAzimuth{};
};

//==============================================================================
struct SourceAudioConfig {
    bool isMuted{};
    tl::optional<output_patch_t> directOut{};
};

//==============================================================================
// struct StereoConfig {
//    StereoMode stereoMode{};
//    output_patch_t leftPatch{};
//    output_patch_t rightPatch{};
//};

//==============================================================================
using SourcesAudioConfig = StaticMap<source_index_t, SourceAudioConfig, MAX_NUM_SOURCES>;
using SpeakersAudioConfig = StaticMap<output_patch_t, SpeakerAudioConfig, MAX_NUM_SPEAKERS>;

//==============================================================================
struct AudioConfig {
    SpatMode spatMode{};
    bool isStereo{};
    float masterGain{};
    float spatGainsInterpolation{};

    juce::Array<std::pair<source_index_t, output_patch_t>> directOutPairs{};

    SourcesAudioConfig sourcesAudioConfig{};
    SpeakersAudioConfig speakersAudioConfig{};

    tl::optional<float> pinkNoiseGain{};

    // LBAP-specific
    LbapAttenuationConfig lbapAttenuationConfig{};
};

//==============================================================================
struct AudioState {
    StrongArray<source_index_t, SourceAudioState, MAX_NUM_SOURCES> sourcesAudioState{};
    StrongArray<output_patch_t, SpeakerAudioState, MAX_NUM_SPEAKERS> speakersAudioState{};
};

//==============================================================================
using SourcePeaks = StrongArray<source_index_t, float, MAX_NUM_SOURCES>;
using SpeakerPeaks = StrongArray<output_patch_t, float, MAX_NUM_SPEAKERS>;
using StereoPeaks = std::array<float, 2>;

//==============================================================================
struct AudioData {
    // message thread -> audio thread (cold)
    std::unique_ptr<AudioConfig> config{};

    // audio thread -> audio thread (hot)
    AudioState state{};

    // audio thread -> message thread (hot)
    AtomicExchanger<SourcePeaks> sourcePeaks{};
    AtomicExchanger<SpeakerPeaks> speakerPeaks{};
    AtomicExchanger<StereoPeaks> stereoPeaks{};
};
