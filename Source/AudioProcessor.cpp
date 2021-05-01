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

#include "AudioProcessor.h"

#include <array>
#include <cstdarg>

#include "AudioManager.h"
#include "MainComponent.h"
#include "PinkNoiseGenerator.h"
#include "StaticMap.hpp"
#include "TaggedAudioBuffer.hpp"
#include "constants.hpp"
#include "narrow.hpp"

float constexpr SMALL_GAIN = 0.0000000000001f;
size_t constexpr MAX_BUFFER_SIZE = 2048;
size_t constexpr LEFT = 0;
size_t constexpr RIGHT = 1;

//==============================================================================
// Load samples from a wav file into a float array.
static juce::AudioBuffer<float> getSamplesFromWavFile(juce::File const & file)
{
    if (!file.existsAsFile()) {
        auto const error{ file.getFullPathName() + "\n\nTry re-installing SpatGRIS." };
        juce::AlertWindow::showMessageBox(juce::AlertWindow::AlertIconType::WarningIcon, "Missing file", error);
        std::exit(-1);
    }

    static auto const FACTOR{ std::pow(2.0f, 31.0f) };

    juce::WavAudioFormat wavAudioFormat{};
    std::unique_ptr<juce::AudioFormatReader> audioFormatReader{
        wavAudioFormat.createReaderFor(file.createInputStream().release(), true)
    };
    jassert(audioFormatReader);
    std::array<int *, 2> wavData{};
    wavData[0] = new int[audioFormatReader->lengthInSamples];
    wavData[1] = new int[audioFormatReader->lengthInSamples];
    audioFormatReader->read(wavData.data(), 2, 0, narrow<int>(audioFormatReader->lengthInSamples), false);
    juce::AudioBuffer<float> samples{ 2, narrow<int>(audioFormatReader->lengthInSamples) };
    for (int i{}; i < 2; ++i) {
        for (int j{}; j < audioFormatReader->lengthInSamples; ++j) {
            samples.setSample(i, j, static_cast<float>(wavData[i][j]) / FACTOR);
        }
    }

    for (auto * it : wavData) {
        delete[] it;
    }
    return samples;
}

//==============================================================================
AudioProcessor::AudioProcessor()
{
    auto & hrtf{ mAudioData.state.hrtf };

    // Initialize impulse responses for VBAP+HRTF (BINAURAL mode).
    // Azimuth = 0
    juce::String names0[8] = { "H0e025a.wav", "H0e020a.wav", "H0e065a.wav", "H0e110a.wav",
                               "H0e155a.wav", "H0e160a.wav", "H0e115a.wav", "H0e070a.wav" };
    int reverse0[8] = { 1, 0, 0, 0, 0, 1, 1, 1 };
    for (int i{}; i < 8; ++i) {
        auto const file{ HRTF_FOLDER_0.getChildFile(names0[i]) };
        auto const buffer{ getSamplesFromWavFile(file) };
        auto const leftChannel{ reverse0[i] };
        auto const rightChannel{ 1 - reverse0[i] };
        std::memcpy(hrtf.leftImpulses[i].data(), buffer.getReadPointer(leftChannel), 128);
        std::memcpy(hrtf.rightImpulses[i].data(), buffer.getReadPointer(rightChannel), 128);
    }
    // Azimuth = 40
    juce::String names40[6]
        = { "H40e032a.wav", "H40e026a.wav", "H40e084a.wav", "H40e148a.wav", "H40e154a.wav", "H40e090a.wav" };
    int reverse40[6] = { 1, 0, 0, 0, 1, 1 };
    for (int i{}; i < 6; ++i) {
        auto const file{ HRTF_FOLDER_40.getChildFile(names40[i]) };
        auto const buffer{ getSamplesFromWavFile(file) };
        auto const leftChannel{ reverse40[i] };
        auto const rightChannel{ 1 - reverse40[i] };
        std::memcpy(hrtf.leftImpulses[i + 8].data(), buffer.getReadPointer(leftChannel), 128);
        std::memcpy(hrtf.rightImpulses[i + 8].data(), buffer.getReadPointer(rightChannel), 128);
    }
    // Azimuth = 80
    for (int i{}; i < 2; ++i) {
        auto const file{ HRTF_FOLDER_80.getChildFile("H80e090a.wav") };
        auto const buffer{ getSamplesFromWavFile(file) };
        auto const leftChannel{ 1 - i };
        auto const rightChannel{ i };
        std::memcpy(hrtf.leftImpulses[i + 14].data(), buffer.getReadPointer(leftChannel), 128);
        std::memcpy(hrtf.rightImpulses[i + 14].data(), buffer.getReadPointer(rightChannel), 128);
    }

    // Initialize pink noise
    srand(static_cast<unsigned>(time(nullptr))); // NOLINT(cert-msc51-cpp)
}

//==============================================================================
void AudioProcessor::resetHrtf()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedLock const lock{ mLock };

    auto & hrtf{ mAudioData.state.hrtf };
    std::fill(hrtf.count.begin(), hrtf.count.end(), 0u);
    static constexpr std::array<float, 128> EMPTY_HRTF_INPUT{};
    std::fill(hrtf.inputTmp.begin(), hrtf.inputTmp.end(), EMPTY_HRTF_INPUT);
}

//==============================================================================
void AudioProcessor::setAudioConfig(std::unique_ptr<AudioConfig> newAudioConfig)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedLock const lock{ mLock };

    if (!mAudioData.config || !mAudioData.config->sourcesAudioConfig.hasSameKeys(newAudioConfig->sourcesAudioConfig)) {
        AudioManager::getInstance().initInputBuffer(newAudioConfig->sourcesAudioConfig.getKeys());
    }
    if (!mAudioData.config
        || !mAudioData.config->speakersAudioConfig.hasSameKeys(newAudioConfig->speakersAudioConfig)) {
        AudioManager::getInstance().initOutputBuffer(newAudioConfig->speakersAudioConfig.getKeys());
    }

    mAudioData.config = std::move(newAudioConfig);

    std::fill(mAudioData.state.sourcesAudioState.begin(), mAudioData.state.sourcesAudioState.end(), SourceAudioState{});
}

//==============================================================================
void AudioProcessor::muteSoloVuMeterIn(SourceAudioBuffer & inputBuffer, SourcePeaks & peaks) const noexcept
{
    for (auto const channel : inputBuffer) {
        auto const & config{ mAudioData.config->sourcesAudioConfig[channel.key] };
        auto const & buffer{ *channel.value };
        auto const peak{ config.isMuted ? 0.0f : buffer.getMagnitude(0, inputBuffer.getNumSamples()) };

        peaks[channel.key] = peak;
    }
}

//==============================================================================
void AudioProcessor::muteSoloVuMeterGainOut(SpeakerAudioBuffer & speakersBuffer, SpeakerPeaks & peaks) noexcept
{
    auto const numSamples{ speakersBuffer.getNumSamples() };

    for (auto const channel : speakersBuffer) {
        auto const & config{ mAudioData.config->speakersAudioConfig[channel.key] };
        auto & buffer{ *channel.value };
        auto const gain{ mAudioData.config->masterGain * config.gain };
        if (config.isMuted || gain < SMALL_GAIN) {
            buffer.clear();
            peaks[channel.key] = 0.0f;
            continue;
        }

        buffer.applyGain(0, numSamples, gain);

        if (config.highpassConfig) {
            auto * const samples{ buffer.getWritePointer(0) };
            auto const & highpassConfig{ *config.highpassConfig };
            auto & highpassVars{ mAudioData.state.speakersAudioState[channel.key].highpassState };
            highpassConfig.process(samples, numSamples, highpassVars);
        }

        auto const magnitude{ buffer.getMagnitude(0, numSamples) };
        peaks[channel.key] = magnitude;
    }
}

//==============================================================================
void AudioProcessor::processVbap(SourceAudioBuffer const & inputBuffer,
                                 SpeakerAudioBuffer & outputBuffer,
                                 SourcePeaks const & sourcePeaks) noexcept
{
    auto const & gainInterpolation{ mAudioData.config->spatGainsInterpolation };
    auto const gainFactor{ std::pow(gainInterpolation, 0.1f) * 0.0099f + 0.99f };

    auto const numSamples{ inputBuffer.getNumSamples() };
    for (auto const & source : mAudioData.config->sourcesAudioConfig) {
        if (source.value.isMuted || source.value.directOut || sourcePeaks[source.key] < SMALL_GAIN) {
            continue;
        }
        auto *& gainsTicket{ mAudioData.state.mostRecentSpatGains[source.key] };
        mAudioData.spatGainMatrix[source.key].getMostRecent(gainsTicket);
        if (gainsTicket == nullptr) {
            continue;
        }
        auto const & gains{ gainsTicket->get() };
        auto & lastGains{ mAudioData.state.sourcesAudioState[source.key].lastSpatGains };
        auto const * inputSamples{ inputBuffer[source.key].getReadPointer(0) };

        for (auto const & speaker : mAudioData.config->speakersAudioConfig) {
            if (speaker.value.isMuted || speaker.value.isDirectOutOnly || speaker.value.gain < SMALL_GAIN) {
                continue;
            }
            auto & currentGain{ lastGains[speaker.key] };
            auto const & targetGain{ gains[speaker.key] };
            auto * outputSamples{ outputBuffer[speaker.key].getWritePointer(0) };
            if (gainInterpolation == 0.0f) {
                // linear interpolation over buffer size
                auto const gainSlope = (targetGain - currentGain) / narrow<float>(numSamples);
                if (targetGain < SMALL_GAIN && currentGain < SMALL_GAIN) {
                    // this is not going to produce any more sounds!
                    continue;
                }
                for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                    currentGain += gainSlope;
                    outputSamples[sampleIndex] += inputSamples[sampleIndex] * currentGain;
                }
            } else {
                // log interpolation with 1st order filter
                for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                    currentGain = targetGain + (currentGain - targetGain) * gainFactor;
                    if (currentGain < SMALL_GAIN && targetGain < SMALL_GAIN) {
                        // If the gain is near zero and the target gain is also near zero, this means that
                        // currentGain will no ever increase over this buffer
                        break;
                    }
                    outputSamples[sampleIndex] += inputSamples[sampleIndex] * currentGain;
                }
            }
        }
    }
}

//==============================================================================
void AudioProcessor::processLbap(SourceAudioBuffer & sourcesBuffer,
                                 SpeakerAudioBuffer & speakersBuffer,
                                 SourcePeaks const & sourcePeaks) noexcept
{
    auto const & gainInterpolation{ mAudioData.config->spatGainsInterpolation };
    auto const gainFactor{ std::pow(gainInterpolation, 0.1f) * 0.0099f + 0.99f };
    auto const numSamples{ sourcesBuffer.getNumSamples() };

    for (auto const & source : mAudioData.config->sourcesAudioConfig) {
        if (source.value.isMuted || source.value.directOut || sourcePeaks[source.key] < SMALL_GAIN) {
            continue;
        }

        auto *& gainsTicket{ mAudioData.state.mostRecentSpatGains[source.key] };
        mAudioData.spatGainMatrix[source.key].getMostRecent(gainsTicket);
        if (gainsTicket == nullptr) {
            continue;
        }
        auto const & gains{ gainsTicket->get() };

        // process attenuation
        auto * inputData{ sourcesBuffer[source.key].getWritePointer(0) };
        mAudioData.config->lbapAttenuationConfig.process(
            inputData,
            numSamples,
            mAudioData.lbapSourceDistances[source.key].load(),
            mAudioData.state.sourcesAudioState[source.key].lbapAttenuationState);

        // Process spatialization
        auto & lastGains{ mAudioData.state.sourcesAudioState[source.key].lastSpatGains };

        for (auto const & speaker : mAudioData.config->speakersAudioConfig) {
            auto * outputSamples{ speakersBuffer[speaker.key].getWritePointer(0) };
            auto const & targetGain{ gains[speaker.key] };
            auto & currentGain{ lastGains[speaker.key] };
            if (gainInterpolation == 0.0f) {
                // linear interpolation over buffer size
                auto const gainSlope = (targetGain - currentGain) / narrow<float>(numSamples);
                if (currentGain < SMALL_GAIN && targetGain < SMALL_GAIN) {
                    // This is not going to produce any more sounds!
                    continue;
                }
                for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                    currentGain += gainSlope;
                    outputSamples[sampleIndex] += inputData[sampleIndex] * currentGain;
                }
            } else {
                // log interpolation with 1st order filter
                for (int sampleIndex{}; sampleIndex < numSamples; ++sampleIndex) {
                    currentGain = (currentGain - targetGain) * gainFactor + targetGain;
                    if (currentGain < SMALL_GAIN && targetGain < SMALL_GAIN) {
                        // If the gain is near zero and the target gain is also near zero, this means that
                        // currentGain will no ever increase over this buffer
                        break;
                    }
                    outputSamples[sampleIndex] += inputData[sampleIndex] * currentGain;
                }
            }
        }
    }
}

//==============================================================================
void AudioProcessor::processVBapHrtf(SourceAudioBuffer const & inputBuffer,
                                     SpeakerAudioBuffer & outputBuffer,
                                     SourcePeaks const & sourcePeaks) noexcept
{
    jassert(outputBuffer.size() == 16);

    processVbap(inputBuffer, outputBuffer, sourcePeaks);

    auto const numSamples{ inputBuffer.getNumSamples() };

    // Process hrtf and mix to stereo
    static std::array<float, MAX_BUFFER_SIZE> leftOutputSamples{};
    static std::array<float, MAX_BUFFER_SIZE> rightOutputSamples{};

    std::fill_n(leftOutputSamples.begin(), numSamples, 0.0f);
    std::fill_n(rightOutputSamples.begin(), numSamples, 0.0f);

    for (auto const & speaker : mAudioData.config->speakersAudioConfig) {
        auto & hrtfState{ mAudioData.state.hrtf };
        auto & hrtfCount{ hrtfState.count };
        auto & hrtfInputTmp{ hrtfState.inputTmp };
        auto const outputIndex{ speaker.key.removeOffset<size_t>() };
        auto const & outputSamplesBuffer{ outputBuffer[speaker.key] };
        if (outputSamplesBuffer.getMagnitude(0, numSamples) == 0.0f) {
            continue;
        }
        auto const * outputSamples{ outputBuffer[speaker.key].getReadPointer(0) };
        auto const & hrtfLeftImpulses{ hrtfState.leftImpulses };
        auto const & hrtfRightImpulses{ hrtfState.rightImpulses };
        for (size_t sampleIndex{}; sampleIndex < narrow<size_t>(numSamples); ++sampleIndex) {
            auto tmpCount{ narrow<int>(hrtfCount[outputIndex]) };
            for (unsigned hrtfIndex{}; hrtfIndex < HRTF_NUM_SAMPLES; ++hrtfIndex) {
                if (tmpCount < 0) {
                    tmpCount += HRTF_NUM_SAMPLES;
                }
                // TODO : traversing tmpCount backwards is probably hurting performances
                auto const sig{ hrtfInputTmp[outputIndex][tmpCount] };
                leftOutputSamples[sampleIndex] += sig * hrtfLeftImpulses[outputIndex][hrtfIndex];
                rightOutputSamples[sampleIndex] += sig * hrtfRightImpulses[outputIndex][hrtfIndex];
                --tmpCount;
            }
            hrtfCount[outputIndex]++;
            if (hrtfCount[outputIndex] >= HRTF_NUM_SAMPLES) {
                hrtfCount[outputIndex] = 0;
            }
            hrtfInputTmp[outputIndex][hrtfCount[outputIndex]] = outputSamples[sampleIndex];
        }
    }

    static constexpr output_patch_t LEFT_OUTPUT_PATCH{ 1 };
    static constexpr output_patch_t RIGHT_OUTPUT_PATCH{ 2 };
    for (auto const & speaker : mAudioData.config->speakersAudioConfig) {
        if (speaker.key == LEFT_OUTPUT_PATCH) {
            outputBuffer[LEFT_OUTPUT_PATCH].copyFrom(0, 0, leftOutputSamples.data(), numSamples);
        } else if (speaker.key == RIGHT_OUTPUT_PATCH) {
            outputBuffer[RIGHT_OUTPUT_PATCH].copyFrom(0, 0, rightOutputSamples.data(), numSamples);
        } else {
            outputBuffer[speaker.key].clear();
        }
    }
}

//==============================================================================
void AudioProcessor::processStereo(SourceAudioBuffer const & inputBuffer,
                                   SpeakerAudioBuffer & outputBuffer,
                                   SourcePeaks const & sourcePeaks) noexcept
{
    jassert(outputBuffer.size() == 2);

    // Vbap does what we're looking for
    processVbap(inputBuffer, outputBuffer, sourcePeaks);

    // Apply gain compensation.
    auto & leftBuffer{ outputBuffer[output_patch_t{ 1 }] };
    auto & rightBuffer{ outputBuffer[output_patch_t{ 2 }] };
    auto const numSamples{ inputBuffer.getNumSamples() };
    auto const compensation{ std::pow(10.0f,
                                      (narrow<float>(mAudioData.config->sourcesAudioConfig.size()) - 1.0f) * -0.005f) };
    leftBuffer.applyGain(0, numSamples, compensation);
    rightBuffer.applyGain(0, numSamples, compensation);
}

//==============================================================================
void AudioProcessor::processAudio(SourceAudioBuffer & sourceBuffer, SpeakerAudioBuffer & speakerBuffer) noexcept
{
    // Skip if the user is editing the speaker setup.
    juce::ScopedTryLock const lock{ mLock };
    if (!lock.isLocked()) {
        return;
    }

    jassert(sourceBuffer.getNumSamples() == speakerBuffer.getNumSamples());
    auto const numSamples{ sourceBuffer.getNumSamples() };

    // Process source peaks
    auto * sourcePeaksTicket{ mAudioData.sourcePeaks.acquire() };
    auto & sourcePeaks{ sourcePeaksTicket->get() };
    muteSoloVuMeterIn(sourceBuffer, sourcePeaks);

    if (mAudioData.config->pinkNoiseGain) {
        // Process pink noise
        StaticVector<output_patch_t, MAX_NUM_SPEAKERS> activeChannels{};
        for (auto const & channel : mAudioData.config->speakersAudioConfig) {
            activeChannels.push_back(channel.key);
        }
        auto data{ speakerBuffer.getArrayOfWritePointers(activeChannels) };
        fillWithPinkNoise(data.data(), numSamples, narrow<int>(data.size()), *mAudioData.config->pinkNoiseGain);
    } else {
        // Process spat algorithm
        switch (mAudioData.config->spatMode) {
        case SpatMode::vbap:
            processVbap(sourceBuffer, speakerBuffer, sourcePeaks);
            break;
        case SpatMode::lbap:
            processLbap(sourceBuffer, speakerBuffer, sourcePeaks);
            break;
        case SpatMode::hrtfVbap:
            processVBapHrtf(sourceBuffer, speakerBuffer, sourcePeaks);
            break;
        case SpatMode::stereo:
            processStereo(sourceBuffer, speakerBuffer, sourcePeaks);
            break;
        default:
            jassertfalse;
            break;
        }

        // Process direct outs
        for (auto const & directOutPair : mAudioData.config->directOutPairs) {
            auto const & origin{ sourceBuffer[directOutPair.first] };
            auto & dest{ speakerBuffer[directOutPair.second] };
            dest.addFrom(0, 0, origin, 0, 0, numSamples);
        }
    }

    // Process speaker peaks/gains/highpass
    auto * speakerPeaksTicket{ mAudioData.speakerPeaks.acquire() };
    auto & speakerPeaks{ speakerPeaksTicket->get() };
    muteSoloVuMeterGainOut(speakerBuffer, speakerPeaks);

    // return peaks data to message thread
    mAudioData.sourcePeaks.setMostRecent(sourcePeaksTicket);
    mAudioData.speakerPeaks.setMostRecent(speakerPeaksTicket);
}

//==============================================================================
AudioProcessor::~AudioProcessor()
{
    AudioManager::getInstance().getAudioDeviceManager().getCurrentAudioDevice()->close();
}
