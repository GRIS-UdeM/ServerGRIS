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

#include "Narrow.hpp"

#include <JuceHeader.h>

//==============================================================================
class StrongIndexBase
{
};

//==============================================================================
template<typename T, typename Dummy, T StartsAt>
class StrongIndex : public StrongIndexBase
{
    T mValue;

    static_assert(std::is_integral_v<T>, "Underlying types should be integrals.");

public:
    //==============================================================================
    using type = T;
    static constexpr auto OFFSET = StartsAt;
    //==============================================================================
    StrongIndex() = default;
    explicit constexpr StrongIndex(type const & value) : mValue(value) {}
    //==============================================================================
    [[nodiscard]] constexpr bool operator==(StrongIndex const & other) const { return mValue == other.mValue; }
    [[nodiscard]] constexpr bool operator!=(StrongIndex const & other) const { return mValue != other.mValue; }
    [[nodiscard]] constexpr bool operator<(StrongIndex const & other) const { return mValue < other.mValue; }
    [[nodiscard]] constexpr bool operator>(StrongIndex const & other) const { return mValue > other.mValue; }
    [[nodiscard]] constexpr bool operator<=(StrongIndex const & other) const { return mValue <= other.mValue; }
    [[nodiscard]] constexpr bool operator>=(StrongIndex const & other) const { return mValue >= other.mValue; }
    //==============================================================================
    [[nodiscard]] constexpr type const & get() const { return mValue; }
    //==============================================================================
    StrongIndex & operator++()
    {
        ++mValue;
        return *this;
    }
    StrongIndex operator++(int) { return StrongIndex{ mValue++ }; }
    StrongIndex & operator--()
    {
        --mValue;
        return *this;
    }
    //==============================================================================
    template<typename TargetType>
    [[nodiscard]] TargetType removeOffset() const
    {
        return narrow<TargetType>(mValue - OFFSET);
    }
};
using source_index_t = StrongIndex<int, struct SourceIndexT, 1>;
using output_patch_t = StrongIndex<int, struct OutputPatchT, 1>;

//==============================================================================
class StrongFloatBase
{
};

//==============================================================================
template<typename T, typename Derived, typename Dummy>
class StrongFloat : public StrongFloatBase
{
    static_assert(std::is_floating_point_v<T>, "Underlying types should be floating points.");
    static_assert(std::is_floating_point_v<T>);

protected:
    //==============================================================================
    T mValue;

public:
    //==============================================================================
    using type = T;
    //==============================================================================
    StrongFloat() = default;
    explicit constexpr StrongFloat(T const & value) : mValue(value) {}
    //==============================================================================
    [[nodiscard]] constexpr bool operator==(Derived const & other) const { return mValue == other.mValue; }
    [[nodiscard]] constexpr bool operator!=(Derived const & other) const { return mValue != other.mValue; }
    [[nodiscard]] constexpr bool operator<(Derived const & other) const { return mValue < other.mValue; }
    [[nodiscard]] constexpr bool operator>(Derived const & other) const { return mValue > other.mValue; }
    [[nodiscard]] constexpr bool operator<=(Derived const & other) const { return mValue <= other.mValue; }
    [[nodiscard]] constexpr bool operator>=(Derived const & other) const { return mValue >= other.mValue; }
    //==============================================================================
    [[nodiscard]] constexpr type const & get() const { return mValue; }
    //==============================================================================
    [[nodiscard]] constexpr Derived operator-() const { return Derived{ -mValue }; }
    [[nodiscard]] constexpr Derived operator+(Derived const & other) const { return Derived{ mValue + other.mValue }; }
    [[nodiscard]] constexpr Derived operator-(Derived const & other) const { return Derived{ mValue - other.mValue }; }
    [[nodiscard]] constexpr Derived operator*(type const mod) const { return Derived{ mValue * mod }; }
    [[nodiscard]] constexpr Derived operator/(type const mod) const { return Derived{ mValue / mod }; }
    [[nodiscard]] constexpr type operator/(Derived const & other) const { return mValue / other.mValue; }
    //==============================================================================
    Derived & operator+=(Derived const & other) noexcept
    {
        mValue += other.mValue;
        return *static_cast<Derived *>(this);
    }
    Derived & operator*=(type const & mod) noexcept
    {
        mValue *= mod;
        return *static_cast<Derived *>(this);
    }
    Derived & operator/=(type const & mod) noexcept
    {
        jassert(mod != 0);
        mValue /= mod;
        return *static_cast<Derived *>(this);
    }
    //==============================================================================
    [[nodiscard]] constexpr Derived abs() const noexcept { return Derived{ std::abs(mValue) }; }

protected:
    //==============================================================================
    [[nodiscard]] constexpr Derived centeredAroundZero(type const amplitude) const noexcept
    {
        auto const halfAmplitude{ amplitude / static_cast<type>(2) };
        auto const isTooSmall = [min = -halfAmplitude](type const value) -> bool { return value < min; };

        if (isTooSmall(mValue)) {
            auto newValue{ mValue + amplitude };
            while (isTooSmall(newValue)) {
                newValue += amplitude;
            }
            return Derived{ newValue };
        }

        auto const isTooHigh = [max = halfAmplitude](type const value) -> bool { return value >= max; };

        if (isTooHigh(mValue)) {
            auto newValue{ mValue - amplitude };
            while (isTooHigh(newValue)) {
                newValue -= amplitude;
            }
            return Derived{ newValue };
        }

        return *static_cast<Derived const *>(this);
    }
};

//==============================================================================
class dbfs_t final : public StrongFloat<float, dbfs_t, struct VolumeT>
{
public:
    dbfs_t() = default;
    explicit constexpr dbfs_t(type const & value) : StrongFloat(value) {}
    //==============================================================================
    [[nodiscard]] type toGain() const { return juce::Decibels::decibelsToGain(mValue); }
    static dbfs_t fromGain(type const gain) { return dbfs_t{ juce::Decibels::gainToDecibels(gain) }; }
};

//==============================================================================
class degrees_t final : public StrongFloat<float, degrees_t, struct DegreesT>
{
public:
    static constexpr type DEGREE_PER_RADIAN{ static_cast<type>(360) / juce::MathConstants<type>::twoPi };
    //==============================================================================
    degrees_t() = default;
    explicit constexpr degrees_t(type const & value) : StrongFloat(value) {}
    //==============================================================================
    [[nodiscard]] constexpr degrees_t centered() const noexcept { return centeredAroundZero(static_cast<type>(360)); }
    [[nodiscard]] constexpr degrees_t madePositive() const noexcept
    {
        return degrees_t{ mValue < 0 ? mValue + static_cast<type>(360) : mValue };
    }
};

//==============================================================================
class radians_t final : public StrongFloat<float, radians_t, struct RadiansT>
{
public:
    static constexpr type RADIAN_PER_DEGREE{ juce::MathConstants<type>::twoPi / static_cast<type>(360) };
    //==============================================================================
    radians_t() = default;
    explicit constexpr radians_t(type const & value) : StrongFloat(value) {}
    constexpr radians_t(degrees_t const & degrees) : StrongFloat(degrees.get() * RADIAN_PER_DEGREE) {}
    //==============================================================================
    [[nodiscard]] constexpr radians_t centered() const noexcept
    {
        return centeredAroundZero(juce::MathConstants<type>::twoPi);
    }
    //==============================================================================
    [[nodiscard]] constexpr radians_t madePositive() const noexcept
    {
        return radians_t{ mValue < 0 ? mValue + juce::MathConstants<type>::twoPi : mValue };
    }
    //==============================================================================
    [[nodiscard]] constexpr degrees_t toDegrees() const noexcept
    {
        return degrees_t{ mValue * degrees_t::DEGREE_PER_RADIAN };
    }
    //==============================================================================
    [[nodiscard]] constexpr operator degrees_t() const { return toDegrees(); }
};

//==============================================================================
class hz_t final : public StrongFloat<float, hz_t, struct HzT>
{
public:
    hz_t() = default;
    explicit constexpr hz_t(type const & value) : StrongFloat(value) {}
};

//==============================================================================
class meters_t : public StrongFloat<float, meters_t, struct MetersT>
{
public:
    meters_t() = default;
    explicit constexpr meters_t(type const & value) : StrongFloat(value) {}
};

//==============================================================================
constexpr radians_t QUARTER_PI{ juce::MathConstants<radians_t::type>::halfPi / 2.0f };
constexpr radians_t HALF_PI{ juce::MathConstants<radians_t::type>::halfPi };
constexpr radians_t PI{ juce::MathConstants<radians_t::type>::pi };
constexpr radians_t TWO_PI{ juce::MathConstants<radians_t::type>::twoPi };
