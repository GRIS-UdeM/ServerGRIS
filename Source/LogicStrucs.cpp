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

#include "LogicStrucs.hpp"

#include "LegacySpatFileFormat.hpp"

juce::String const SourceData::XmlTags::STATE = "STATE";
juce::String const SourceData::XmlTags::DIRECT_OUT = "DIRECT_OUT";
juce::String const SourceData::XmlTags::COLOUR = "COLOR";
juce::String const SourceData::XmlTags::MAIN_TAG_PREFIX = "SOURCE_";

juce::String const SpeakerHighpassData::XmlTags::MAIN_TAG = "HIGHPASS";
juce::String const SpeakerHighpassData::XmlTags::FREQ = "FREQ";

juce::String const SpeakerData::XmlTags::STATE = "STATE";
juce::String const SpeakerData::XmlTags::GAIN = "GAIN";
juce::String const SpeakerData::XmlTags::IS_DIRECT_OUT_ONLY = "DIRECT_OUT_ONLY";
juce::String const SpeakerData::XmlTags::MAIN_TAG_PREFIX = "SPEAKER_";

juce::String const LbapDistanceAttenuationData::XmlTags::MAIN_TAG = "LBAP_SETTINGS";
juce::String const LbapDistanceAttenuationData::XmlTags::FREQ = "FREQ";
juce::String const LbapDistanceAttenuationData::XmlTags::ATTENUATION = "ATTENUATION";

juce::String const AudioSettings::XmlTags::MAIN_TAG = "AUDIO_SETTINGS";
juce::String const AudioSettings::XmlTags::INTERFACE_TYPE = "INTERFACE_TYPE";
juce::String const AudioSettings::XmlTags::INPUT_INTERFACE = "INPUT_INTERFACE";
juce::String const AudioSettings::XmlTags::OUTPUT_INTERFACE = "OUTPUT_INTERFACE";
juce::String const AudioSettings::XmlTags::SAMPLE_RATE = "SAMPLE_RATE";
juce::String const AudioSettings::XmlTags::BUFFER_SIZE = "BUFFER_SIZE";

juce::String const RecordingOptions::XmlTags::MAIN_TAG = "RECORDING_OPTIONS";
juce::String const RecordingOptions::XmlTags::FORMAT = "FORMAT";
juce::String const RecordingOptions::XmlTags::FILE_TYPE = "FILE_TYPE";

juce::String const StereoRouting::XmlTags::MAIN_TAG = "STEREO_ROUTING";
juce::String const StereoRouting::XmlTags::LEFT = "LEFT";
juce::String const StereoRouting::XmlTags::RIGHT = "RIGHT";

juce::String const ViewSettings::XmlTags::MAIN_TAG = "VIEW_SETTINGS";
juce::String const ViewSettings::XmlTags::SHOW_SPEAKERS = "SHOW_SPEAKERS";
juce::String const ViewSettings::XmlTags::SHOW_SPEAKER_NUMBERS = "SHOW_SPEAKER_NUMBERS";
juce::String const ViewSettings::XmlTags::SHOW_SPEAKER_TRIPLETS = "SHOW_SPEAKER_TRIPLETS";
juce::String const ViewSettings::XmlTags::SHOW_SPEAKER_LEVELS = "SHOW_SPEAKER_LEVELS";
juce::String const ViewSettings::XmlTags::SHOW_SPHERE_OR_CUBE = "SHOW_SPHERE_OR_CUBE";
juce::String const ViewSettings::XmlTags::SHOW_SOURCE_ACTIVITY = "SHOW_SOURCE_ACTIVITY";

juce::String const SpatGrisProjectData::XmlTags::MAIN_TAG = "SPAT_GRIS_PROJECT_DATA";
juce::String const SpatGrisProjectData::XmlTags::VERSION = "VERSION";
juce::String const SpatGrisProjectData::XmlTags::SOURCES = "SOURCES";
juce::String const SpatGrisProjectData::XmlTags::MASTER_GAIN = "MASTER_GAIN";
juce::String const SpatGrisProjectData::XmlTags::GAIN_INTERPOLATION = "GAIN_INTERPOLATION";
juce::String const SpatGrisProjectData::XmlTags::OSC_PORT = "OSC_PORT";

juce::String const SpatGrisAppData::XmlTags::MAIN_TAG = "SPAT_GRIS_APP_DATA";
juce::String const SpatGrisAppData::XmlTags::LAST_SPEAKER_SETUP = "LAST_SPEAKER_SETUP";
juce::String const SpatGrisAppData::XmlTags::LAST_PROJECT = "LAST_PROJECT";
juce::String const SpatGrisAppData::XmlTags::LAST_RECORDING_DIRECTORY = "LAST_RECORDING_DIRECTORY";
juce::String const SpatGrisAppData::XmlTags::LAST_STEREO_MODE = "LAST_STEREO_MODE";
juce::String const SpatGrisAppData::XmlTags::WINDOW_X = "WINDOW_X";
juce::String const SpatGrisAppData::XmlTags::WINDOW_Y = "WINDOW_Y";
juce::String const SpatGrisAppData::XmlTags::WINDOW_WIDTH = "WINDOW_WIDTH";
juce::String const SpatGrisAppData::XmlTags::WINDOW_HEIGHT = "WINDOW_HEIGHT";
juce::String const SpatGrisAppData::XmlTags::SASH_POSITION = "SASH_POSITION";
juce::String const SpatGrisAppData::XmlTags::CAMERA = "CAMERA";

juce::String const SpeakerSetup::XmlTags::MAIN_TAG = "SPEAKER_SETUP";
juce::String const SpeakerSetup::XmlTags::VERSION = "VERSION";
juce::String const SpeakerSetup::XmlTags::SPAT_MODE = "SPAT_MODE";

//==============================================================================
juce::String portStateToString(PortState const state)
{
    switch (state) {
    case PortState::muted:
        return "muted";
    case PortState::solo:
        return "solo";
    case PortState::normal:
        return "normal";
    }
    jassertfalse;
    return "";
}

//==============================================================================
tl::optional<PortState> stringToPortState(juce::String const & string)
{
    if (string == "muted") {
        return PortState::muted;
    }
    if (string == "solo") {
        return PortState::solo;
    }
    if (string == "normal") {
        return PortState::normal;
    }
    return tl::nullopt;
}

//==============================================================================
SourceAudioConfig SourceData::toConfig(bool const soloMode) const
{
    SourceAudioConfig result;
    result.directOut = directOut;
    result.isMuted = soloMode ? state != PortState::solo : state == PortState::muted;
    return result;
}

//==============================================================================
ViewportSourceData SourceData::toViewportData(float const alpha) const
{
    jassert(position);

    ViewportSourceData result;
    result.colour = colour.withAlpha(alpha);
    result.azimuthSpan = azimuthSpan;
    result.zenithSpan = zenithSpan;
    result.position = *position;

    return result;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> SourceData::toXml(source_index_t const index) const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG_PREFIX + juce::String{ index.get() }) };

    result->setAttribute(XmlTags::STATE, portStateToString(state));
    if (directOut) {
        result->setAttribute(XmlTags::DIRECT_OUT, directOut->get());
    }
    result->setAttribute(XmlTags::COLOUR, juce::String{ colour.getARGB() });

    return result;
}

//==============================================================================
tl::optional<SourceData> SourceData::fromXml(juce::XmlElement const & xml)
{
    if (!xml.getTagName().startsWith(XmlTags::MAIN_TAG_PREFIX)) {
        return tl::nullopt;
    }

    juce::StringArray const requiredTags{ XmlTags::STATE, XmlTags::COLOUR };

    if (!std::all_of(requiredTags.begin(), requiredTags.end(), [&](juce::String const & tag) {
            return xml.hasAttribute(tag);
        })) {
        return tl::nullopt;
    }

    auto const state{ stringToPortState(xml.getStringAttribute(XmlTags::STATE)) };

    if (!state) {
        return tl::nullopt;
    }

    SourceData result{};
    result.state = *state;
    if (xml.hasAttribute(XmlTags::DIRECT_OUT)) {
        result.directOut = output_patch_t{ xml.getIntAttribute(XmlTags::DIRECT_OUT) };
    }
    result.colour = juce::Colour{ narrow<uint32_t>(xml.getStringAttribute(XmlTags::COLOUR).getLargeIntValue()) };

    return result;
}

//==============================================================================
bool SourceData::operator==(SourceData const & other) const noexcept
{
    // TODO : this is very misleading and should be replaced with something that does make it seem like we're really
    // comparing the two sources.
    return other.directOut == directOut && other.colour == colour;
}

//==============================================================================
SpeakerHighpassConfig SpeakerHighpassData::toConfig(double const sampleRate) const
{
    auto const f{ narrow<double>(freq.get()) };
    auto const wc{ 2.0 * juce::MathConstants<double>::pi * f };
    auto const wc2{ wc * wc };
    auto const wc3{ wc2 * wc };
    auto const wc4{ wc2 * wc2 };
    auto const k{ wc / std::tan(juce::MathConstants<double>::pi * f / sampleRate) };
    auto const k2{ k * k };
    auto const k3{ k2 * k };
    auto const k4{ k2 * k2 };
    auto const sqTmp1{ juce::MathConstants<double>::sqrt2 * wc3 * k };
    auto const sqTmp2{ juce::MathConstants<double>::sqrt2 * wc * k3 };
    auto const aTmp{ 4.0 * wc2 * k2 + 2.0 * sqTmp1 + k4 + 2.0 * sqTmp2 + wc4 };
    auto const k4ATmp{ k4 / aTmp };

    /* common */
    auto const b1{ (4.0 * (wc4 + sqTmp1 - k4 - sqTmp2)) / aTmp };
    auto const b2{ (6.0 * wc4 - 8.0 * wc2 * k2 + 6.0 * k4) / aTmp };
    auto const b3{ (4.0 * (wc4 - sqTmp1 + sqTmp2 - k4)) / aTmp };
    auto const b4{ (k4 - 2.0 * sqTmp1 + wc4 - 2.0 * sqTmp2 + 4.0 * wc2 * k2) / aTmp };

    /* highpass */
    auto const ha0{ k4ATmp };
    auto const ha1{ -4.0 * k4ATmp };
    auto const ha2{ 6.0 * k4ATmp };

    return SpeakerHighpassConfig{ b1, b2, b3, b4, ha0, ha1, ha2 };
}

//==============================================================================
std::unique_ptr<juce::XmlElement> SpeakerHighpassData::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };
    result->setAttribute(XmlTags::FREQ, freq.get());
    return result;
}

//==============================================================================
tl::optional<SpeakerHighpassData> SpeakerHighpassData::fromXml(juce::XmlElement const & xml)
{
    if (xml.getTagName() != XmlTags::MAIN_TAG || !xml.hasAttribute(XmlTags::FREQ)) {
        return tl::nullopt;
    }

    SpeakerHighpassData result;
    result.freq = hz_t{ static_cast<float>(xml.getDoubleAttribute(XmlTags::FREQ)) };

    return result;
}

//==============================================================================
bool SpeakerHighpassData::operator==(SpeakerHighpassData const & other) const noexcept
{
    return other.freq == freq;
}

//==============================================================================
SpeakerAudioConfig SpeakerData::toConfig(bool const soloMode, double const sampleRate) const noexcept
{
    auto const getHighpassConfig = [&](SpeakerHighpassData const & data) { return data.toConfig(sampleRate); };

    SpeakerAudioConfig result;
    result.isMuted = soloMode ? state != PortState::solo : state == PortState::muted;
    result.gain = gain.toGain();
    result.highpassConfig = highpassData.map(getHighpassConfig);
    result.isDirectOutOnly = isDirectOutOnly;
    return result;
}

//==============================================================================
ViewportSpeakerConfig SpeakerData::toViewportConfig() const noexcept
{
    return ViewportSpeakerConfig{ position, isSelected, isDirectOutOnly };
}

//==============================================================================
std::unique_ptr<juce::XmlElement> SpeakerData::toXml(output_patch_t const outputPatch) const noexcept
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG_PREFIX + juce::String{ outputPatch.get() }) };

    result->setAttribute(XmlTags::STATE, portStateToString(state));
    result->addChildElement(position.getCartesian().toXml());
    result->setAttribute(XmlTags::GAIN, gain.get());
    if (highpassData) {
        result->addChildElement(highpassData->toXml().release());
    }
    result->setAttribute(XmlTags::IS_DIRECT_OUT_ONLY, isDirectOutOnly);

    return result;
}

//==============================================================================
tl::optional<SpeakerData> SpeakerData::fromXml(juce::XmlElement const & xml) noexcept
{
    juce::StringArray const requiredTags{ XmlTags::GAIN, XmlTags::IS_DIRECT_OUT_ONLY, XmlTags::STATE };

    auto const * positionElement{ xml.getChildByName(CartesianVector::XmlTags::MAIN_TAG) };

    if (positionElement == nullptr
        || !std::all_of(requiredTags.begin(), requiredTags.end(), [&](juce::String const & tag) {
               return xml.hasAttribute(tag);
           })) {
        return tl::nullopt;
    }

    auto const position{ CartesianVector::fromXml(*positionElement) };
    auto const state{ stringToPortState(xml.getStringAttribute(XmlTags::STATE)) };

    if (!position || !state) {
        return tl::nullopt;
    }

    auto const * crossoverElement{ xml.getChildByName(SpeakerHighpassData::XmlTags::MAIN_TAG) };

    SpeakerData result{};
    result.state = *state;
    result.position = *position;
    result.gain = dbfs_t{ static_cast<float>(xml.getDoubleAttribute(XmlTags::GAIN)) };
    if (crossoverElement) {
        auto const crossover{ SpeakerHighpassData::fromXml(*crossoverElement) };
        if (!crossover) {
            return tl::nullopt;
        }
        result.highpassData = crossover;
    }
    result.isDirectOutOnly = xml.getBoolAttribute(XmlTags::IS_DIRECT_OUT_ONLY);

    return result;
}

//==============================================================================
bool SpeakerData::operator==(SpeakerData const & other) const noexcept
{
    return other.isDirectOutOnly == isDirectOutOnly && other.highpassData == highpassData && other.position == position
           && other.gain == gain;
}

//==============================================================================
LbapAttenuationConfig LbapDistanceAttenuationData::toConfig(double const sampleRate) const
{
    auto const coefficient{ std::exp(-juce::MathConstants<float>::twoPi * freq.get() / narrow<float>(sampleRate)) };
    auto const gain{ attenuation.toGain() };
    LbapAttenuationConfig const result{ gain, coefficient };
    return result;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> LbapDistanceAttenuationData::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    result->setAttribute(XmlTags::FREQ, freq.get());
    result->setAttribute(XmlTags::ATTENUATION, attenuation.get());

    return result;
}

//==============================================================================
tl::optional<LbapDistanceAttenuationData> LbapDistanceAttenuationData::fromXml(juce::XmlElement const & xml)
{
    if (xml.getTagName() != XmlTags::MAIN_TAG || !xml.hasAttribute(XmlTags::FREQ)
        || !xml.hasAttribute(XmlTags::ATTENUATION)) {
        return tl::nullopt;
    }

    LbapDistanceAttenuationData result{};
    result.freq = hz_t{ static_cast<float>(xml.getDoubleAttribute(XmlTags::FREQ)) };
    result.attenuation = dbfs_t{ static_cast<float>(xml.getDoubleAttribute(XmlTags::ATTENUATION)) };

    return result;
}

//==============================================================================
bool LbapDistanceAttenuationData::operator==(LbapDistanceAttenuationData const & other) const noexcept
{
    return other.attenuation == attenuation && other.freq == freq;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> AudioSettings::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    result->setAttribute(XmlTags::INTERFACE_TYPE, deviceType);
    result->setAttribute(XmlTags::INPUT_INTERFACE, inputDevice);
    result->setAttribute(XmlTags::OUTPUT_INTERFACE, outputDevice);
    result->setAttribute(XmlTags::SAMPLE_RATE, sampleRate);
    result->setAttribute(XmlTags::BUFFER_SIZE, bufferSize);

    return result;
}

//==============================================================================
tl::optional<AudioSettings> AudioSettings::fromXml(juce::XmlElement const & xml)
{
    juce::StringArray requiredAttributes{ XmlTags::INTERFACE_TYPE,
                                          XmlTags::INPUT_INTERFACE,
                                          XmlTags::OUTPUT_INTERFACE,
                                          XmlTags::SAMPLE_RATE,
                                          XmlTags::BUFFER_SIZE };

    if (xml.getTagName() != XmlTags::MAIN_TAG
        || !std::all_of(requiredAttributes.begin(), requiredAttributes.end(), [&](juce::String const & string) {
               return xml.hasAttribute(string);
           })) {
        return tl::nullopt;
    }

    tl::optional<AudioSettings> result{ AudioSettings{} };
    result->deviceType = xml.getStringAttribute(XmlTags::INTERFACE_TYPE);
    result->inputDevice = xml.getStringAttribute(XmlTags::INPUT_INTERFACE);
    result->outputDevice = xml.getStringAttribute(XmlTags::OUTPUT_INTERFACE);
    result->sampleRate = xml.getDoubleAttribute(XmlTags::SAMPLE_RATE);
    result->bufferSize = xml.getIntAttribute(XmlTags::BUFFER_SIZE);

    return result;
}

//==============================================================================
juce::String recordingFormatToString(RecordingFormat const format)
{
    return RECORDING_FORMAT_STRINGS[narrow<int>(format)];
}

//==============================================================================
tl::optional<RecordingFormat> stringToRecordingFormat(juce::String const & string)
{
    for (int i{}; i < RECORDING_FORMAT_STRINGS.size(); ++i) {
        if (string == RECORDING_FORMAT_STRINGS[i]) {
            return narrow<RecordingFormat>(i);
        }
    }

    return tl::nullopt;
}

//==============================================================================
juce::String recordingFileTypeToString(RecordingFileType const fileType)
{
    return RECORDING_FILE_TYPE_STRINGS[narrow<int>(fileType)];
}

//==============================================================================
tl::optional<RecordingFileType> stringToRecordingFileType(juce::String const & string)
{
    for (int i{}; i < RECORDING_FILE_TYPE_STRINGS.size(); ++i) {
        if (string == RECORDING_FILE_TYPE_STRINGS[i]) {
            return narrow<RecordingFileType>(i);
        }
    }
    return tl::nullopt;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> RecordingOptions::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    result->setAttribute(XmlTags::FORMAT, recordingFormatToString(format));
    result->setAttribute(XmlTags::FILE_TYPE, recordingFileTypeToString(fileType));

    return result;
}

//==============================================================================
tl::optional<RecordingOptions> RecordingOptions::fromXml(juce::XmlElement const & xml)
{
    auto const format{ stringToRecordingFormat(xml.getStringAttribute(XmlTags::FORMAT)) };
    auto const fileType{ stringToRecordingFileType(xml.getStringAttribute(XmlTags::FILE_TYPE)) };

    if (!format || !fileType) {
        return tl::nullopt;
    }

    RecordingOptions result;
    result.format = *format;
    result.fileType = *fileType;

    return result;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> StereoRouting::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    result->setAttribute(XmlTags::LEFT, left.get());
    result->setAttribute(XmlTags::RIGHT, right.get());

    return result;
}

//==============================================================================
tl::optional<StereoRouting> StereoRouting::fromXml(juce::XmlElement const & xml)
{
    if (xml.getTagName() != XmlTags::MAIN_TAG || !xml.hasAttribute(XmlTags::LEFT)
        || !xml.hasAttribute(XmlTags::RIGHT)) {
        return tl::nullopt;
    }

    StereoRouting result;
    result.left = output_patch_t{ xml.getIntAttribute(XmlTags::LEFT, 1) };
    result.right = output_patch_t{ xml.getIntAttribute(XmlTags::RIGHT, 2) };
    return result;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> ViewSettings::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    result->setAttribute(XmlTags::SHOW_SPEAKERS, showSpeakers);
    result->setAttribute(XmlTags::SHOW_SPEAKER_NUMBERS, showSpeakerNumbers);
    result->setAttribute(XmlTags::SHOW_SPEAKER_TRIPLETS, showSpeakerTriplets);
    result->setAttribute(XmlTags::SHOW_SPEAKER_LEVELS, showSpeakerLevels);
    result->setAttribute(XmlTags::SHOW_SPHERE_OR_CUBE, showSphereOrCube);
    result->setAttribute(XmlTags::SHOW_SOURCE_ACTIVITY, showSourceActivity);

    return result;
}

//==============================================================================
tl::optional<ViewSettings> ViewSettings::fromXml(juce::XmlElement const & xml)
{
    juce::StringArray const requiredTags{ XmlTags::SHOW_SPEAKERS,         XmlTags::SHOW_SPEAKER_NUMBERS,
                                          XmlTags::SHOW_SPEAKER_TRIPLETS, XmlTags::SHOW_SPEAKER_LEVELS,
                                          XmlTags::SHOW_SPHERE_OR_CUBE,   XmlTags::SHOW_SOURCE_ACTIVITY };

    if (xml.getTagName() != XmlTags::MAIN_TAG
        || !std::all_of(requiredTags.begin(), requiredTags.end(), [&](juce::String const & tag) {
               return xml.hasAttribute(tag);
           })) {
        return tl::nullopt;
    }

    ViewSettings result;
    result.showSpeakers = xml.getBoolAttribute(XmlTags::SHOW_SPEAKERS);
    result.showSpeakerNumbers = xml.getBoolAttribute(XmlTags::SHOW_SPEAKER_NUMBERS);
    result.showSpeakerTriplets = xml.getBoolAttribute(XmlTags::SHOW_SPEAKER_TRIPLETS);
    result.showSpeakerLevels = xml.getBoolAttribute(XmlTags::SHOW_SPEAKER_LEVELS);
    result.showSphereOrCube = xml.getBoolAttribute(XmlTags::SHOW_SPHERE_OR_CUBE);
    result.showSourceActivity = xml.getBoolAttribute(XmlTags::SHOW_SOURCE_ACTIVITY);

    return result;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> SpatGrisProjectData::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    auto sourcesElement{ std::make_unique<juce::XmlElement>(XmlTags::SOURCES) };
    for (auto const sourceData : sources) {
        sourcesElement->addChildElement(sourceData.value->toXml(sourceData.key).release());
    }

    result->addChildElement(sourcesElement.release());
    result->addChildElement(lbapDistanceAttenuationData.toXml().release());

    result->setAttribute(XmlTags::OSC_PORT, oscPort);
    result->setAttribute(XmlTags::MASTER_GAIN, masterGain.get());
    result->setAttribute(XmlTags::GAIN_INTERPOLATION, spatGainsInterpolation);
    result->setAttribute(XmlTags::VERSION, SPAT_GRIS_VERSION.toString());

    return result;
}

//==============================================================================
tl::optional<SpatGrisProjectData> SpatGrisProjectData::fromXml(juce::XmlElement const & xml)
{
    juce::StringArray const requiredTags{ XmlTags::MASTER_GAIN, XmlTags::GAIN_INTERPOLATION, XmlTags::OSC_PORT };
    if (xml.getTagName() != XmlTags::MAIN_TAG
        || !std::all_of(requiredTags.begin(), requiredTags.end(), [&](juce::String const & string) {
               return xml.hasAttribute(string);
           })) {
        return readLegacyProjectFile(xml);
    }

    auto const * sourcesElement{ xml.getChildByName(XmlTags::SOURCES) };
    auto const * lbapAttenuationElement{ xml.getChildByName(LbapDistanceAttenuationData::XmlTags::MAIN_TAG) };

    if (!sourcesElement || !lbapAttenuationElement) {
        return tl::nullopt;
    }

    SpatGrisProjectData result{};

    auto const lbapAttenuation{ LbapDistanceAttenuationData::fromXml(*lbapAttenuationElement) };

    if (!lbapAttenuation) {
        return tl::nullopt;
    }

    result.masterGain = LEGAL_MASTER_GAIN_RANGE.clipValue(
        dbfs_t{ static_cast<dbfs_t::type>(xml.getDoubleAttribute(XmlTags::MASTER_GAIN)) });
    result.spatGainsInterpolation = LEGAL_GAIN_INTERPOLATION_RANGE.clipValue(
        static_cast<float>(xml.getDoubleAttribute(XmlTags::GAIN_INTERPOLATION)));
    result.oscPort = xml.getIntAttribute(XmlTags::OSC_PORT); // TODO : validate value
    result.lbapDistanceAttenuationData = *lbapAttenuation;

    forEachXmlChildElement(*sourcesElement, sourceElement)
    {
        jassert(sourceElement);
        auto const sourceData{ SourceData::fromXml(*sourceElement) };
        if (!sourceData) {
            return tl::nullopt;
        }

        auto const sourceIndex{ source_index_t{
            sourceElement->getTagName().substring(SourceData::XmlTags::MAIN_TAG_PREFIX.length()).getIntValue() } };
        jassert(LEGAL_SOURCE_INDEX_RANGE.contains(sourceIndex));
        if (!LEGAL_SOURCE_INDEX_RANGE.contains(sourceIndex)) {
            return tl::nullopt;
        }
        result.sources.add(sourceIndex, std::make_unique<SourceData>(*sourceData));
    }

    return tl::make_optional(std::move(result));
}

//==============================================================================
bool SpatGrisProjectData::operator==(SpatGrisProjectData const & other) const noexcept
{
    return other.spatGainsInterpolation == spatGainsInterpolation && other.oscPort == oscPort
           && other.masterGain == masterGain && other.lbapDistanceAttenuationData == lbapDistanceAttenuationData
           && other.sources == sources;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> SpatGrisAppData::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    auto cameraElement{ std::make_unique<juce::XmlElement>(XmlTags::CAMERA) };
    cameraElement->addChildElement(cameraPosition.toXml());

    result->addChildElement(audioSettings.toXml().release());
    result->addChildElement(recordingOptions.toXml().release());
    result->addChildElement(cameraElement.release());
    result->addChildElement(viewSettings.toXml().release());
    result->addChildElement(stereoRouting.toXml().release());

    result->setAttribute(XmlTags::LAST_SPEAKER_SETUP, lastSpeakerSetup);
    result->setAttribute(XmlTags::LAST_PROJECT, lastProject);
    result->setAttribute(XmlTags::LAST_RECORDING_DIRECTORY, lastRecordingDirectory);
    result->setAttribute(XmlTags::WINDOW_X, windowX);
    result->setAttribute(XmlTags::WINDOW_Y, windowY);
    result->setAttribute(XmlTags::WINDOW_WIDTH, windowWidth);
    result->setAttribute(XmlTags::WINDOW_HEIGHT, windowHeight);
    result->setAttribute(XmlTags::SASH_POSITION, sashPosition);

    if (stereoMode) {
        result->setAttribute(XmlTags::LAST_STEREO_MODE, stereoModeToString(*stereoMode));
    }

    return result;
}

//==============================================================================
tl::optional<SpatGrisAppData> SpatGrisAppData::fromXml(juce::XmlElement const & xml)
{
    juce::StringArray const requiredTags{
        XmlTags::LAST_SPEAKER_SETUP, XmlTags::LAST_PROJECT, XmlTags::LAST_RECORDING_DIRECTORY,
        XmlTags::WINDOW_X,           XmlTags::WINDOW_Y,     XmlTags::WINDOW_WIDTH,
        XmlTags::WINDOW_HEIGHT,      XmlTags::SASH_POSITION
    };

    auto const * audioSettingsElement{ xml.getChildByName(AudioSettings::XmlTags::MAIN_TAG) };
    auto const * recordingOptionsElement{ xml.getChildByName(RecordingOptions::XmlTags::MAIN_TAG) };
    auto const * cameraElement{ xml.getChildByName(XmlTags::CAMERA) };
    auto const * viewSettingsElement{ xml.getChildByName(ViewSettings::XmlTags::MAIN_TAG) };
    auto const * stereoRoutingElement{ xml.getChildByName(StereoRouting::XmlTags::MAIN_TAG) };

    if (xml.getTagName() != XmlTags::MAIN_TAG || !audioSettingsElement || !recordingOptionsElement || !cameraElement
        || !viewSettingsElement || !stereoRoutingElement
        || !std::all_of(requiredTags.begin(), requiredTags.end(), [&](juce::String const & tag) {
               return xml.hasAttribute(tag);
           })) {
        return tl::nullopt;
    }

    auto const * cameraPositionElement{ cameraElement->getChildByName(CartesianVector::XmlTags::MAIN_TAG) };
    if (!cameraPositionElement) {
        return tl::nullopt;
    }

    auto const audioSettings{ AudioSettings::fromXml(*audioSettingsElement) };
    auto const recordingOptions{ RecordingOptions::fromXml(*recordingOptionsElement) };
    auto const cameraPosition{ CartesianVector::fromXml(*cameraPositionElement) };
    auto const viewSettings{ ViewSettings::fromXml(*viewSettingsElement) };
    auto const lastStereoMode{ stringToStereoMode(xml.getStringAttribute(XmlTags::LAST_STEREO_MODE)) };
    auto const stereoRouting{ StereoRouting::fromXml(*stereoRoutingElement) };

    if (!audioSettings || !recordingOptions || !cameraPosition || !viewSettings || !stereoRouting) {
        return tl::nullopt;
    }

    SpatGrisAppData result;

    result.audioSettings = *audioSettings;
    result.recordingOptions = *recordingOptions;
    result.stereoMode = lastStereoMode;
    result.stereoRouting = *stereoRouting;

    result.lastSpeakerSetup = xml.getStringAttribute(XmlTags::LAST_SPEAKER_SETUP);
    result.lastProject = xml.getStringAttribute(XmlTags::LAST_PROJECT);
    result.lastRecordingDirectory = xml.getStringAttribute(XmlTags::LAST_RECORDING_DIRECTORY);
    result.windowX = xml.getIntAttribute(XmlTags::WINDOW_X);
    result.windowY = xml.getIntAttribute(XmlTags::WINDOW_Y);
    result.windowWidth = xml.getIntAttribute(XmlTags::WINDOW_WIDTH);
    result.windowHeight = xml.getIntAttribute(XmlTags::WINDOW_HEIGHT);
    result.sashPosition = xml.getDoubleAttribute(XmlTags::SASH_POSITION);
    result.viewSettings = *viewSettings;
    result.cameraPosition = *cameraPosition;

    return result;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> SpeakerSetup::toXml() const
{
    auto result{ std::make_unique<juce::XmlElement>(XmlTags::MAIN_TAG) };

    result->setAttribute(XmlTags::VERSION, SPAT_GRIS_VERSION.toString());
    result->setAttribute(XmlTags::SPAT_MODE, spatModeToString(spatMode));

    jassert(ordering.size() == speakers.size());
    for (auto const outputPatch : ordering) {
        result->addChildElement(speakers[outputPatch].toXml(outputPatch).release());
    }

    return result;
}

//==============================================================================
tl::optional<SpeakerSetup> SpeakerSetup::fromXml(juce::XmlElement const & xml)
{
    auto const spatMode{ stringToSpatMode(xml.getStringAttribute(XmlTags::SPAT_MODE)) };

    if (xml.getTagName() != XmlTags::MAIN_TAG || !spatMode) {
        return readLegacySpeakerSetup(xml);
    }

    tl::optional<SpeakerSetup> result{ SpeakerSetup{} };
    result->spatMode = *spatMode;

    forEachXmlChildElement(xml, speaker)
    {
        auto const tagName{ speaker->getTagName() };
        if (!tagName.startsWith(SpeakerData::XmlTags::MAIN_TAG_PREFIX)) {
            return tl::nullopt;
        }
        output_patch_t const outputPatch{
            tagName.substring(SpeakerData::XmlTags::MAIN_TAG_PREFIX.length()).getIntValue()
        };
        result->ordering.add(outputPatch);
        auto const speakerData{ SpeakerData::fromXml(*speaker) };
        if (!speakerData) {
            return tl::nullopt;
        }

        result->speakers.add(outputPatch, std::make_unique<SpeakerData>(*speakerData));
    }

    return result;
}

//==============================================================================
bool SpeakerSetup::operator==(SpeakerSetup const & other) const noexcept
{
    return other.ordering == ordering && other.speakers == speakers && other.spatMode == spatMode;
}

//==============================================================================
bool SpeakerSetup::isDomeLike() const noexcept
{
    return std::all_of(speakers.cbegin(), speakers.cend(), [](SpeakersData::ConstNode const & node) {
        return node.value->isDirectOutOnly || juce::isWithin(node.value->position.getPolar().length, 1.0f, 0.02f);
    });
}

//==============================================================================
SpeakersAudioConfig SpeakerSetup::toAudioConfig(double const sampleRate) const noexcept
{
    SpeakersAudioConfig result{};

    auto const isAtLeastOnSpeakerSolo{ std::any_of(speakers.cbegin(), speakers.cend(), [](auto const node) {
        return node.value->state == PortState::solo;
    }) };

    for (auto const speaker : speakers) {
        result.add(speaker.key, speaker.value->toConfig(isAtLeastOnSpeakerSolo, sampleRate));
    }

    return result;
}

//==============================================================================
int SpeakerSetup::numOfSpatializedSpeakers() const noexcept
{
    auto const result{ std::count_if(speakers.cbegin(), speakers.cend(), [](SpeakersData::ConstNode const & speaker) {
        return !speaker.value->isDirectOutOnly;
    }) };

    return narrow<int>(result);
}

//==============================================================================
std::unique_ptr<AudioConfig> SpatGrisData::toAudioConfig() const
{
    auto result{ std::make_unique<AudioConfig>() };

    result->speakersAudioConfig = speakerSetup.toAudioConfig(appData.audioSettings.sampleRate);

    auto const isAtLeastOneSourceSolo{ std::any_of(
        project.sources.cbegin(),
        project.sources.cend(),
        [](auto const node) { return node.value->state == PortState::solo; }) };

    auto const isValidDirectOut = [&](SourceData const & source) {
        if (!source.directOut) {
            return false;
        }

        if (source.state == PortState::muted) {
            return false;
        }

        if (isAtLeastOneSourceSolo && source.state != PortState::solo) {
            return false;
        }

        if (!speakerSetup.speakers.contains(*source.directOut)) {
            return false;
        }

        return true;
    };

    for (auto const & source : project.sources) {
        if (isValidDirectOut(*source.value)) {
            result->directOutPairs.add(std::make_pair(source.key, *source.value->directOut));
        }
    }

    result->lbapAttenuationConfig = project.lbapDistanceAttenuationData.toConfig(appData.audioSettings.sampleRate);
    result->masterGain = project.masterGain.toGain();
    result->pinkNoiseGain = pinkNoiseLevel.map([](auto const & level) { return level.toGain(); });
    for (auto const source : project.sources) {
        result->sourcesAudioConfig.add(source.key, source.value->toConfig(isAtLeastOneSourceSolo));
    }
    result->spatMode = speakerSetup.spatMode;
    result->spatGainsInterpolation = project.spatGainsInterpolation;

    if (appData.stereoMode) {
        result->isStereo = appData.stereoMode.has_value();

        // direct outs are ignored in stereo mode
        result->directOutPairs.clearQuick();

        for (auto & source : result->sourcesAudioConfig) {
            source.value.directOut.reset();
        }
    }

    return result;
}

//==============================================================================
ViewportConfig SpatGrisData::toViewportConfig() const noexcept
{
    ViewportConfig result{};
    for (auto const & speaker : speakerSetup.speakers) {
        result.speakers.add(speaker.key, speaker.value->toViewportConfig());
    }
    result.spatMode = speakerSetup.spatMode;
    result.viewSettings = appData.viewSettings;
    result.title = juce::File{ appData.lastSpeakerSetup }.getFileNameWithoutExtension();

    return result;
}
