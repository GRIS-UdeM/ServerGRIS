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

#include "LegacyLbapPosition.h"

//==============================================================================
PolarVector LegacyLbapPosition::toPolar() const noexcept
{
    return PolarVector::fromCartesian(toCartesian());
}

//==============================================================================
CartesianVector LegacyLbapPosition::toCartesian() const noexcept
{
    auto const x{ floorDistance * std::cos(azimuth.get()) };
    auto const y{ floorDistance * std::sin(azimuth.get()) };
    auto const z{ 1.0f - (HALF_PI - elevation) / HALF_PI };

    return CartesianVector{ x, y, z };
}