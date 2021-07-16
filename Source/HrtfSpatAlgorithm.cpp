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

#include "HrtfSpatAlgorithm.hpp"

#include "DummySpatAlgorithm.hpp"
#include "LbapSpatAlgorithm.hpp"
#include "StaticMap.hpp"
#include "StrongArray.hpp"
#include "TaggedAudioBuffer.hpp"
#include "VbapSpatAlgorithm.hpp"

static constexpr size_t MAX_BUFFER_SIZE = 2048;

//==============================================================================
HrtfSpatAlgorithm::HrtfSpatAlgorithm(SpeakerSetup const & speakerSetup,
                                     SourcesData const & sources,
                                     double const sampleRate,
                                     int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    static juce::StringArray const NAMES{ "H0e025a.wav",  "H0e020a.wav",  "H0e065a.wav",  "H0e110a.wav",
                                          "H0e155a.wav",  "H0e160a.wav",  "H0e115a.wav",  "H0e070a.wav",
                                          "H40e032a.wav", "H40e026a.wav", "H40e084a.wav", "H40e148a.wav",
                                          "H40e154a.wav", "H40e090a.wav", "H80e090a.wav", "H80e090a.wav" };
    static auto const GET_HRTF_IR_FILE = [](int const speaker) {
        jassert(juce::isPositiveAndBelow(speaker, NAMES.size()));

        auto const & name{ NAMES[speaker] };

        if (speaker < 8) {
            return HRTF_FOLDER_0.getChildFile(name);
        }

        if (speaker < 14) {
            return HRTF_FOLDER_40.getChildFile(name);
        }

        return HRTF_FOLDER_80.getChildFile(name);
    };

    static auto const GET_HRTF_IR_FILES = []() {
        juce::Array<juce::File> files{};
        for (int i{}; i < NAMES.size(); ++i) {
            files.add(GET_HRTF_IR_FILE(i));
        }

        return files;
    };

    static auto const FILES{ GET_HRTF_IR_FILES() };

    // Init inner spat algorithm
    juce::Array<output_patch_t> hrtfPatches{};
    auto const binauralXml{ juce::XmlDocument{ BINAURAL_SPEAKER_SETUP_FILE }.getDocumentElement() };
    jassert(binauralXml);
    auto const binauralSpeakerSetup{ SpeakerSetup::fromXml(*binauralXml) };
    jassert(binauralSpeakerSetup);
    mHrtfData.speakersAudioConfig
        = binauralSpeakerSetup->toAudioConfig(44100.0); // TODO: find a way to update this number!
    auto speakers{ binauralSpeakerSetup->ordering };
    speakers.sort();
    mHrtfData.speakersBuffer.init(speakers);

    auto const & binauralSpeakerData{ binauralSpeakerSetup->speakers };

    switch (speakerSetup.spatMode) {
    case SpatMode::vbap:
        mInnerAlgorithm = std::make_unique<VbapSpatAlgorithm>(binauralSpeakerData);
        break;
    case SpatMode::lbap:
        mInnerAlgorithm = std::make_unique<LbapSpatAlgorithm>(binauralSpeakerData);
        break;
    }

    jassert(mInnerAlgorithm);

    // load IRs
    for (int i{}; i < 16; ++i) {
        mConvolutions[i].loadImpulseResponse(FILES[i],
                                             juce::dsp::Convolution::Stereo::yes,
                                             juce::dsp::Convolution::Trim::no,
                                             0,
                                             juce::dsp::Convolution::Normalise::no);
    }

    juce::dsp::ProcessSpec const spec{ sampleRate, narrow<juce::uint32>(bufferSize), 2 };
    for (auto & convolution : mConvolutions) {
        convolution.prepare(spec);
        convolution.reset();
    }

    fixDirectOutsIntoPlace(sources, speakerSetup);
}

//==============================================================================
void HrtfSpatAlgorithm::updateSpatData(source_index_t const sourceIndex, SourceData const & sourceData) noexcept
{
    jassert(!isProbablyAudioThread());

    if (sourceData.directOut) {
        return;
    }

    mInnerAlgorithm->updateSpatData(sourceIndex, sourceData);
}

//==============================================================================
void HrtfSpatAlgorithm::process(AudioConfig const & config,
                                SourceAudioBuffer & sourcesBuffer,
                                SpeakerAudioBuffer & speakersBuffer,
                                juce::AudioBuffer<float> & stereoBuffer,
                                SourcePeaks const & sourcePeaks,
                                [[maybe_unused]] SpeakersAudioConfig const * altSpeakerConfig)
{
    ASSERT_AUDIO_THREAD;
    jassert(!altSpeakerConfig);
    jassert(stereoBuffer.getNumChannels() == 2);

    speakersBuffer.silence();

    auto & hrtfBuffer{ mHrtfData.speakersBuffer };
    jassert(hrtfBuffer.size() == 16);
    hrtfBuffer.silence();

    mInnerAlgorithm
        ->process(config, sourcesBuffer, hrtfBuffer, stereoBuffer, sourcePeaks, &mHrtfData.speakersAudioConfig);

    auto const numSamples{ sourcesBuffer.getNumSamples() };

    static juce::AudioBuffer<float> convolutionBuffer{};
    convolutionBuffer.setSize(2, numSamples);

    size_t speakerIndex{};
    static constexpr std::array<bool, 16> REVERSE{ true, false, false, false, false, true, true, true,
                                                   true, false, false, false, true,  true, true, false };
    for (auto const & speaker : mHrtfData.speakersAudioConfig) {
        auto const magnitude{ hrtfBuffer[speaker.key].getMagnitude(0, numSamples) };
        auto & hadSoundLastBlock{ mHrtfData.hadSoundLastBlock[speaker.key] };

        // We can skip the speaker if the gain is small enough, but we have to perform one last block so that the
        // convolution's inner state stays coherent.
        if (magnitude <= SMALL_GAIN) {
            if (!hadSoundLastBlock) {
                speakerIndex++;
                continue;
            }
            hadSoundLastBlock = false;
        } else {
            hadSoundLastBlock = true;
        }

        convolutionBuffer.copyFrom(0, 0, hrtfBuffer[speaker.key], 0, 0, numSamples);
        convolutionBuffer.copyFrom(1, 0, hrtfBuffer[speaker.key], 0, 0, numSamples);
        juce::dsp::AudioBlock<float> block{ convolutionBuffer };
        juce::dsp::ProcessContextReplacing<float> const context{ block };
        mConvolutions[speakerIndex].process(context);
        // auto const & result{ context.getOutputBlock() };
        if (!REVERSE[speakerIndex]) {
            stereoBuffer.addFrom(0, 0, convolutionBuffer, 0, 0, numSamples);
            stereoBuffer.addFrom(1, 0, convolutionBuffer, 1, 0, numSamples);
        } else {
            stereoBuffer.addFrom(0, 0, convolutionBuffer, 1, 0, numSamples);
            stereoBuffer.addFrom(1, 0, convolutionBuffer, 0, 0, numSamples);
        }

        speakerIndex++;
    }
}

//==============================================================================
juce::Array<Triplet> HrtfSpatAlgorithm::getTriplets() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassertfalse;
    return juce::Array<Triplet>{};
}

//==============================================================================
std::unique_ptr<AbstractSpatAlgorithm> HrtfSpatAlgorithm::make(SpeakerSetup const & speakerSetup,
                                                               SourcesData const & sources,
                                                               double const sampleRate,
                                                               int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return std::make_unique<HrtfSpatAlgorithm>(speakerSetup, sources, sampleRate, bufferSize);
}
