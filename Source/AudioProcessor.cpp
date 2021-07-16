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

#include "AudioProcessor.hpp"

#include "AudioManager.hpp"
#include "MainComponent.hpp"
#include "Narrow.hpp"
#include "PinkNoiseGenerator.hpp"
#include "StaticMap.hpp"
#include "StrongArray.hpp"
#include "TaggedAudioBuffer.hpp"
#include "constants.hpp"

#include <array>

//==============================================================================
AudioProcessor::AudioProcessor()
{
    // Initialize pink noise
    srand(static_cast<unsigned>(time(nullptr))); // NOLINT(cert-msc51-cpp)
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
void AudioProcessor::processAudio(SourceAudioBuffer & sourceBuffer,
                                  SpeakerAudioBuffer & speakerBuffer,
                                  juce::AudioBuffer<float> & stereoBuffer) noexcept
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
        mSpatAlgorithm->process(*mAudioData.config, sourceBuffer, speakerBuffer, stereoBuffer, sourcePeaks, nullptr);

        // Process direct outs
        for (auto const & directOutPair : mAudioData.config->directOutPairs) {
            auto const & origin{ sourceBuffer[directOutPair.first] };
            auto & dest{ speakerBuffer[directOutPair.second] };
            dest.addFrom(0, 0, origin, 0, 0, numSamples);
        }
    }

    // Process peaks/gains/highpass
    mAudioData.sourcePeaks.setMostRecent(sourcePeaksTicket);

    if (mAudioData.config->isStereo) {
        auto const & masterGain{ mAudioData.config->masterGain };
        if (masterGain != 0.0f) {
            stereoBuffer.applyGain(masterGain);
        }
        auto * stereoPeaksTicket{ mAudioData.stereoPeaks.acquire() };
        auto & stereoPeaks{ stereoPeaksTicket->get() };
        for (int i{}; i < 2; ++i) {
            stereoPeaks[narrow<size_t>(i)] = stereoBuffer.getMagnitude(i, 0, numSamples);
        }
        mAudioData.stereoPeaks.setMostRecent(stereoPeaksTicket);
    } else {
        auto * speakerPeaksTicket{ mAudioData.speakerPeaks.acquire() };
        auto & speakerPeaks{ speakerPeaksTicket->get() };
        // Process speaker peaks/gains/highpass
        muteSoloVuMeterGainOut(speakerBuffer, speakerPeaks);
        mAudioData.speakerPeaks.setMostRecent(speakerPeaksTicket);
    }
}

//==============================================================================
AudioProcessor::~AudioProcessor()
{
    AudioManager::getInstance().getAudioDeviceManager().getCurrentAudioDevice()->close();
}
