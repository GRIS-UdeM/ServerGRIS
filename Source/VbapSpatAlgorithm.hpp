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

#pragma once

#include "vbap.hpp"

#include "AbstractSpatAlgorithm.hpp"

VbapType getVbapType(SpeakersData const & speakers);

//==============================================================================
class VbapSpatAlgorithm final : public AbstractSpatAlgorithm
{
    std::unique_ptr<VbapData> mData{};

public:
    //==============================================================================
    explicit VbapSpatAlgorithm(SpeakersData const & speakers);
    //==============================================================================
    void computeSpeakerGains(SourceData const & source, SpeakersSpatGains & gains) const noexcept override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override;
};
