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

#include "LegacySpatFileFormat.h"

#include "LegacyLbapPosition.h"

//==============================================================================
tl::optional<std::pair<SpeakerSetup, SpatMode>> readLegacySpeakerSetup(juce::XmlElement const & xml)
{
    if (!xml.hasTagName("SpeakerSetup")) {
        return tl::nullopt;
    }

    auto const spatMode{ static_cast<SpatMode>(xml.getIntAttribute("SpatMode")) };
    jassert(spatMode == SpatMode::hrtfVbap || spatMode == SpatMode::lbap || spatMode == SpatMode::vbap
            || spatMode == SpatMode::stereo);
    if (spatMode != SpatMode::hrtfVbap && spatMode != SpatMode::lbap && spatMode != SpatMode::vbap
        && spatMode != SpatMode::stereo) {
        return tl::nullopt;
    }
    juce::Array<std::pair<int, output_patch_t>> layout;
    SpeakerSetup result{};

    forEachXmlChildElement(xml, ring)
    {
        if (ring->hasTagName("Ring")) {
            forEachXmlChildElement(*ring, spk)
            {
                if (spk->hasTagName("Speaker")) {
                    // layout
                    auto const layoutIndex{ spk->getIntAttribute("LayoutIndex") - 1 };
                    output_patch_t outputPatch{ spk->getIntAttribute("OutputPatch") };
                    jassert(LEGAL_OUTPUT_PATCH_RANGE.contains(outputPatch));
                    if (!LEGAL_OUTPUT_PATCH_RANGE.contains(outputPatch)) {
                        return tl::nullopt;
                    }

                    // position
                    radians_t const azimuth{ (degrees_t{ static_cast<float>(-spk->getDoubleAttribute("Azimuth", 0.0)) }
                                              + degrees_t{ 90.0f })
                                                 .centered() };
                    radians_t const zenith{
                        degrees_t{ static_cast<float>(spk->getDoubleAttribute("Zenith", 0.0)) }.centered()
                    };
                    auto const length{ static_cast<float>(spk->getDoubleAttribute("Radius", 1.0)) };
                    auto const vector{ spatMode == SpatMode::lbap
                                           ? LegacyLbapPosition{ azimuth, zenith, length }.toPolar()
                                           : PolarVector{ azimuth, zenith, length } };

                    // audio params
                    dbfs_t const gain{ static_cast<float>(spk->getDoubleAttribute("Gain", 0.0)) };
                    hz_t const highpass{ static_cast<float>(spk->getDoubleAttribute("HighPassCutoff", 0.0)) };
                    auto const isDirectOutOnly{ spk->getBoolAttribute("DirectOut", false) };

                    // build data
                    auto speakerData{ std::make_unique<SpeakerData>() };
                    speakerData->vector = vector;
                    speakerData->position = vector.toCartesian();
                    speakerData->gain = gain;
                    speakerData->highpassData
                        = (highpass == hz_t{} ? tl::optional<SpeakerHighpassData>{} : SpeakerHighpassData{ highpass });
                    speakerData->isDirectOutOnly = isDirectOutOnly;

                    layout.add(std::make_pair(layoutIndex, outputPatch));
                    result.speakers.add(outputPatch, std::move(speakerData));
                }
            }
        }
    }

    std::sort(layout.begin(), layout.end());
    auto const isLayoutValid = [&]() {
        // Layout should be numbers 0 to n
        int expected{};
        for (auto const & node : layout) {
            if (node.first != expected++) {
                return false;
            }
        }

        // output patches should all be existing speakers
        return std::all_of(layout.begin(), layout.end(), [&](std::pair<int, output_patch_t> const & node) {
            return result.speakers.contains(node.second);
        });
    };
    jassert(isLayoutValid());
    if (!isLayoutValid()) {
        return tl::nullopt;
    }

    result.order.resize(layout.size());
    std::transform(layout.begin(),
                   layout.end(),
                   result.order.begin(),
                   [](std::pair<int, output_patch_t> const & indexOutputPair) { return indexOutputPair.second; });

    return std::pair(std::move(result), spatMode);
}

//==============================================================================
tl::optional<SpatGrisProjectData> readLegacyProjectFile(juce::XmlElement const & xml)
{
    if (!xml.hasTagName("SpatServerGRIS_Preset") && !xml.hasTagName("ServerGRIS_Preset")) {
        return tl::nullopt;
    }

    auto const oscPort{ xml.getIntAttribute("OSC_Input_Port", DEFAULT_OSC_INPUT_PORT) }; // TODO : validate value

    // auto const numInputs{ xml.getStringAttribute("Number_Of_Inputs").getIntValue() }; /* UNUSED */

    auto const masterGain{ LEGAL_MASTER_GAIN_RANGE.clipValue(
        dbfs_t{ static_cast<float>(xml.getDoubleAttribute("Master_Gain_Out", 0.0)) }) };
    auto const gainInterpolation{ LEGAL_GAIN_INTERPOLATION_RANGE.clipValue(
        static_cast<float>(xml.getDoubleAttribute("Master_Interpolation", 0.1))) };

    SpatGrisProjectData result{};

    forEachXmlChildElement(xml, source)
    {
        if (source->hasTagName("Input")) {
            source_index_t const index{ source->getIntAttribute("Index") };
            jassert(LEGAL_SOURCE_INDEX_RANGE.contains(index));
            if (!LEGAL_SOURCE_INDEX_RANGE.contains(index)) {
                return tl::nullopt;
            }
            auto const red{ static_cast<float>(source->getDoubleAttribute("R", 1.0f)) };
            auto const green{ static_cast<float>(source->getDoubleAttribute("G", 1.0f)) };
            auto const blue{ static_cast<float>(source->getDoubleAttribute("B", 1.0f)) };
            auto const color{ juce::Colour::fromFloatRGBA(red, green, blue, 1.0f) };
            tl::optional<output_patch_t> directOut{};
            if (source->getIntAttribute("DirectOut") != 0) {
                directOut = output_patch_t{ source->getIntAttribute("DirectOut") };
                jassert(LEGAL_OUTPUT_PATCH_RANGE.contains(*directOut));
                if (!LEGAL_OUTPUT_PATCH_RANGE.contains(*directOut)) {
                    return tl::nullopt;
                }
            }

            auto newSourceData{ std::make_unique<SourceData>() };
            newSourceData->colour = color;
            newSourceData->directOut = directOut;

            result.sources.add(index, std::move(newSourceData));
        }
    }

    result.oscPort = oscPort;
    result.masterGain = masterGain;
    result.spatGainsInterpolation = gainInterpolation;

    return result;
}
