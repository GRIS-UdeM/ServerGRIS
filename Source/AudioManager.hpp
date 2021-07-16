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
#include "LogicStrucs.hpp"
#include "TaggedAudioBuffer.hpp"

#include <JuceHeader.h>

class AudioProcessor;

extern juce::BigInteger const NEEDED_INPUT_CHANNELS;
extern juce::BigInteger const NEEDED_OUTPUT_CHANNELS;

//==============================================================================
class AudioManager final : juce::AudioSourcePlayer
{
    // 2^17 samples * 32 bits per sample == 0.5 mb buffer per channel
    static constexpr auto RECORDERS_BUFFER_SIZE_IN_SAMPLES = 131072;
    //==============================================================================
    struct RecorderInfo {
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter{};
        juce::AudioFormatWriter *
            audioFormatWriter{}; // this is only left for safety assertions : it will get deleted by the threadedWriter
        juce::Array<float const *> dataToRecord{};
    };

public:
    //==============================================================================
    struct RecordingParameters {
        juce::String path{};
        RecordingOptions options{};
        double sampleRate{};
        juce::Array<output_patch_t> speakersToRecord{};
    };

private:
    //==============================================================================
    AudioProcessor * mAudioProcessor{};
    AudioConfig const * mAudioConfigRef{};
    juce::AudioDeviceManager mAudioDeviceManager{};
    SourceAudioBuffer mInputBuffer{};
    SpeakerAudioBuffer mOutputBuffer{};
    juce::AudioBuffer<float> mStereoOutputBuffer{};
    tl::optional<StereoRouting> mStereoRouting{};
    // Recording
    bool mIsRecording{};
    juce::Atomic<int64_t> mNumSamplesRecorded{};
    juce::OwnedArray<RecorderInfo> mRecorders{};
    juce::TimeSliceThread mRecordersThread{ "SpatGRIS recording thread" };
    //==============================================================================
    static std::unique_ptr<AudioManager> mInstance;

public:
    //==============================================================================
    ~AudioManager();
    //==============================================================================
    AudioManager(AudioManager const &) = delete;
    AudioManager(AudioManager &&) = delete;
    AudioManager & operator=(AudioManager const &) = delete;
    AudioManager & operator=(AudioManager &&) = delete;
    //==============================================================================
    [[nodiscard]] juce::AudioDeviceManager const & getAudioDeviceManager() const { return mAudioDeviceManager; }
    [[nodiscard]] juce::AudioDeviceManager & getAudioDeviceManager() { return mAudioDeviceManager; }

    juce::StringArray getAvailableDeviceTypeNames();

    void registerAudioProcessor(AudioProcessor * audioProcessor);

    bool prepareToRecord(RecordingParameters const & recordingParams);
    void startRecording();
    void stopRecording();
    bool isRecording() const { return mIsRecording; }
    int64_t getNumSamplesRecorded() const { return mNumSamplesRecorded.get(); }

    void initInputBuffer(juce::Array<source_index_t> const & sources);
    void initOutputBuffer(juce::Array<output_patch_t> const & speakers);
    void setBufferSize(int newBufferSize);
    void setStereoRouting(tl::optional<StereoRouting> const & stereoRouting);
    //==============================================================================
    // AudioSourcePlayer overrides
    void audioDeviceError(const juce::String & errorMessage) override;
    void audioDeviceIOCallback(const float ** inputChannelData,
                               int totalNumInputChannels,
                               float ** outputChannelData,
                               int totalNumOutputChannels,
                               int numSamples) override;
    void audioDeviceAboutToStart(juce::AudioIODevice * device) override;
    void audioDeviceStopped() override;
    //==============================================================================
    static void init(juce::String const & deviceType,
                     juce::String const & inputDevice,
                     juce::String const & outputDevice,
                     double sampleRate,
                     int bufferSize);
    static void free();
    [[nodiscard]] static AudioManager & getInstance();

private:
    //==============================================================================
    AudioManager(juce::String const & deviceType,
                 juce::String const & inputDevice,
                 juce::String const & outputDevice,
                 double sampleRate,
                 int bufferSize);
    //==============================================================================
    [[nodiscard]] bool tryInitAudioDevice(juce::String const & deviceType,
                                          juce::String const & inputDevice,
                                          juce::String const & outputDevice,
                                          double requestedSampleRate,
                                          int requestedBufferSize);
    //==============================================================================
    JUCE_LEAK_DETECTOR(AudioManager)
}; // class AudioManager
