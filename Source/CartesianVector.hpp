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

#include <JuceHeader.h>

#include "lib/tl/optional.hpp"

#include <cmath>

#include <limits>

// constexpr sqrt from Alex Shtof
// https://stackoverflow.com/questions/8622256/in-c11-is-sqrt-defined-as-constexpr
namespace Detail
{
double constexpr sqrtNewtonRaphson(double const x, double const curr, double const prev)
{
    return curr == prev ? curr : sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
}
} // namespace Detail

/*
 * Constexpr version of the square root
 * Return value:
 *   - For a finite and non-negative value of "x", returns an approximation for the square root of "x"
 *   - Otherwise, returns NaN
 */
double constexpr constexpr_sqrt(double const x)
{
    return x >= 0 && x < std::numeric_limits<double>::infinity() ? Detail::sqrtNewtonRaphson(x, x, 0)
                                                                 : std::numeric_limits<double>::quiet_NaN();
}

struct CartesianVector {
    struct XmlTags {
        static juce::String const MAIN_TAG;
        static juce::String const X;
        static juce::String const Y;
        static juce::String const Z;
    };

    float x;
    float y;
    float z;

    [[nodiscard]] constexpr bool operator==(CartesianVector const & other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }

    [[nodiscard]] constexpr CartesianVector operator-(CartesianVector const & other) const noexcept
    {
        CartesianVector const result{ x - other.x, y - other.y, z - other.z };
        return result;
    }

    [[nodiscard]] constexpr CartesianVector operator/(float const scalar) const noexcept
    {
        CartesianVector const result{ x / scalar, y / scalar, z / scalar };
        return result;
    }

    /* Returns the vector length without the sqrt. */
    [[nodiscard]] constexpr float length2() const noexcept { return x * x + y * y + z * z; }

    [[nodiscard]] float length() const noexcept { return std::sqrt(length2()); }

    [[nodiscard]] constexpr float constexpr_length() const noexcept
    {
        return static_cast<float>(constexpr_sqrt(length2()));
    }

    [[nodiscard]] CartesianVector crossProduct(CartesianVector const & other) const noexcept;

    [[nodiscard]] constexpr CartesianVector operator-() const noexcept { return CartesianVector{ -x, -y, -z }; }

    [[nodiscard]] constexpr float dotProduct(CartesianVector const & other) const noexcept
    {
        return x * other.x + y * other.y + z * other.z;
    }

    [[nodiscard]] constexpr CartesianVector mean(CartesianVector const & other) const noexcept
    {
        auto const newX{ (x + other.x) * 0.5f };
        auto const newY{ (y + other.y) * 0.5f };
        auto const newZ{ (z + other.z) * 0.5f };

        CartesianVector const result{ newX, newY, newZ };
        return result;
    }

    [[nodiscard]] float angleWith(CartesianVector const & other) const noexcept;

    [[nodiscard]] juce::XmlElement * toXml() const noexcept
    {
        auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };
        result->setAttribute(XmlTags::X, x);
        result->setAttribute(XmlTags::Y, y);
        result->setAttribute(XmlTags::Z, z);
        return result.release();
    }

    [[nodiscard]] juce::Point<float> discardZ() const noexcept { return juce::Point<float>{ x, y }; }

    [[nodiscard]] static tl::optional<CartesianVector> fromXml(juce::XmlElement const & xml)
    {
        juce::StringArray const requiredTags{ XmlTags::X, XmlTags::Y, XmlTags::Z };

        if (!std::all_of(requiredTags.begin(), requiredTags.end(), [&](juce::String const & tag) {
                return xml.hasAttribute(tag);
            })) {
            return tl::nullopt;
        }

        CartesianVector result;
        result.x = static_cast<float>(xml.getDoubleAttribute(XmlTags::X));
        result.y = static_cast<float>(xml.getDoubleAttribute(XmlTags::Y));
        result.z = static_cast<float>(xml.getDoubleAttribute(XmlTags::Z));

        return result;
    }
};