/*
 * Functions for 3D VBAP processing based on work by Ville Pulkki.
 * (c) Ville Pulkki - 2.2.1999 Helsinki University of Technology.
 * Updated by belangeo, 2017.
 * Updated by Samuel Béland, 2021.
 */

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

#include "AudioStructs.hpp"
#include "CartesianVector.hpp"
#include "LogicStrucs.hpp"
#include "PolarVector.hpp"
#include "StrongTypes.hpp"
#include "constants.hpp"

#include <JuceHeader.h>

struct SourceData;
constexpr auto MIN_VOL_P_SIDE_LENGTH = 0.01f;

using fast = juce::dsp::FastMathApproximations;

struct SpeakersSetup {
    int dimension;         /* Number of dimension, always 3. */
    int count;             /* Number of speakers. */
    degrees_t * azimuth;   /* Azimuth angle of speakers. */
    degrees_t * elevation; /* Elevation angle of speakers. */
};

using InverseMatrix = std::array<float, 9>;

/* A struct for a loudspeaker triplet or pair (set). */
struct SpeakerSet {
    std::array<output_patch_t, 3> speakerNos;
    InverseMatrix invMx;
    std::array<float, 3> setGains;
    float smallestWt;
    int neg_g_am;
};

/* VBAP structure of n loudspeaker panning */
struct VbapData {
    std::array<output_patch_t, MAX_NUM_SPEAKERS> outputPatches{}; /* Physical outputs (starts at 1). */
    std::array<float, MAX_NUM_SPEAKERS> gainsSmoothing{};         /* Loudspeaker gains smoothing. */
    int dimension{};                                              /* Dimensions, 2 or 3. */
    juce::Array<SpeakerSet> speakerSets{};                        /* Loudspeaker triplet structure. */
    int numOutputPatches{};                                       /* Number of output patches. */
    int numSpeakers{};                                            /* Number of loudspeakers. */
    Position direction{};
    CartesianVector spreadingVector{}; /* Spreading vector. */
};

std::unique_ptr<VbapData> vbapInit(std::array<Position, MAX_NUM_SPEAKERS> & speakers,
                                   int count,
                                   int dimensions,
                                   std::array<output_patch_t, MAX_NUM_SPEAKERS> const & outputPatches);

/* Calculates gain factors using loudspeaker setup and angle direction.
 */
void vbapCompute(SourceData const & source, SpeakersSpatGains & gains, VbapData & data) noexcept;

juce::Array<Triplet> vbapExtractTriplets(VbapData const & data);
