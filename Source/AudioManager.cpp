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

#include "AudioManager.hpp"

#include "AudioProcessor.hpp"
#include "constants.hpp"

// #define SIMULATE_NO_AUDIO_DEVICES

juce::BigInteger const NEEDED_INPUT_CHANNELS{ [] {
    juce::BigInteger channels{};
    channels.setRange(0, MAX_NUM_SOURCES, true);
    return channels;
}() };

juce::BigInteger const NEEDED_OUTPUT_CHANNELS{ [] {
    juce::BigInteger channels{};
    channels.setRange(0, MAX_NUM_SPEAKERS, true);
    return channels;
}() };

//==============================================================================
std::unique_ptr<AudioManager> AudioManager::mInstance{ nullptr };

//==============================================================================
AudioManager::AudioManager(juce::String const & deviceType,
                           juce::String const & inputDevice,
                           juce::String const & outputDevice,

                           double const sampleRate,
                           int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;

#ifndef SIMULATE_NO_AUDIO_DEVICES
    auto const success{ tryInitAudioDevice(deviceType, inputDevice, outputDevice, sampleRate, bufferSize) };

    if (!success) {
        mAudioDeviceManager.initialiseWithDefaultDevices(MAX_NUM_SOURCES, MAX_NUM_SPEAKERS);

        auto setup{ mAudioDeviceManager.getAudioDeviceSetup() };
        setup.inputChannels = NEEDED_INPUT_CHANNELS;
        setup.outputChannels = NEEDED_OUTPUT_CHANNELS;
        if (mAudioDeviceManager.setAudioDeviceSetup(setup, true).isNotEmpty()) {
            mAudioDeviceManager.initialiseWithDefaultDevices(MAX_NUM_SOURCES, MAX_NUM_SPEAKERS);
        }
    }
#endif

    mRecordersThread.setPriority(9);
    mAudioDeviceManager.addAudioCallback(this);
}

//==============================================================================
void AudioManager::audioDeviceIOCallback(float const ** inputChannelData,
                                         int const totalNumInputChannels,
                                         float ** const outputChannelData,
                                         int const totalNumOutputChannels,
                                         int const numSamples)
{
    jassert(numSamples <= mInputBuffer.MAX_NUM_SAMPLES);
    jassert(numSamples <= mOutputBuffer.MAX_NUM_SAMPLES);

    if (!mAudioProcessor) {
        return;
    }

    // clear buffers
    mInputBuffer.silence();
    mOutputBuffer.silence();
    mStereoOutputBuffer.clear();
    std::for_each_n(outputChannelData, totalNumOutputChannels, [numSamples](float * const data) {
        std::fill_n(data, numSamples, 0.0f);
    });

    juce::ScopedTryLock const lock{ mAudioProcessor->getLock() };
    if (!lock.isLocked()) {
        return;
    }

    // copy input data to buffers
    auto const numInputChannelsToCopy{ std::min(totalNumInputChannels, mInputBuffer.size()) };
    for (int i{}; i < numInputChannelsToCopy; ++i) {
        source_index_t const sourceIndex{ i + 1 };
        auto const * sourceData{ inputChannelData[i] };
        auto * destinationData{ mInputBuffer[sourceIndex].getWritePointer(0) };
        std::copy_n(sourceData, numSamples, destinationData);
    }

    // do the actual processing
    if (mAudioProcessor != nullptr) {
        mAudioProcessor->processAudio(mInputBuffer, mOutputBuffer, mStereoOutputBuffer);
    }

    // copy buffers to output
    mOutputBuffer.copyToPhysicalOutput(outputChannelData, totalNumOutputChannels);

    // Record
    if (mIsRecording) {
        auto const stopRecordingAndDisplayError = [this]() {
            stopRecording();
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::AlertIconType::WarningIcon,
                "Error",
                "Recording stopped because samples were dropped.\nRecording on a faster disk might solve this issue.");
        };

        for (auto const & recorder : mRecorders) {
            jassert(recorder->audioFormatWriter->getNumChannels() == recorder->dataToRecord.size());
            auto const success{ recorder->threadedWriter->write(recorder->dataToRecord.data(), numSamples) };
            jassert(success);
            if (!success) {
                stopRecordingAndDisplayError();
            }
        }
        mNumSamplesRecorded += numSamples;
    }
}

//==============================================================================
bool AudioManager::tryInitAudioDevice(juce::String const & deviceType,
                                      juce::String const & inputDevice,
                                      juce::String const & outputDevice,
                                      double const requestedSampleRate,
                                      int const requestedBufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (deviceType.isEmpty() || inputDevice.isEmpty() || outputDevice.isEmpty()) {
        return false;
    }

    auto const & availableDeviceTypes{ mAudioDeviceManager.getAvailableDeviceTypes() };

    auto const * const foundDeviceType{ std::find_if(
        std::cbegin(availableDeviceTypes),
        std::cend(availableDeviceTypes),
        [&deviceType](juce::AudioIODeviceType const * audioDeviceType) -> bool {
            return audioDeviceType->getTypeName() == deviceType;
        }) };
    if (foundDeviceType == std::cend(availableDeviceTypes)) {
        return false;
    }

    mAudioDeviceManager.setCurrentAudioDeviceType(deviceType, true);
    auto * deviceTypeObject{ mAudioDeviceManager.getCurrentDeviceTypeObject() };
    jassert(deviceTypeObject);
    deviceTypeObject->scanForDevices();

    auto const filterDevice = [&](juce::String const & name, bool const isInput) -> juce::String {
        auto const names{ deviceTypeObject->getDeviceNames(isInput) };
        if (names.contains(name)) {
            return name;
        }

        return names[deviceTypeObject->getDefaultDeviceIndex(isInput)];
    };

    auto const filteredInputDevice{ filterDevice(inputDevice, true) };
    auto const filteredOutputDevice{ filterDevice(outputDevice, false) };

    juce::AudioDeviceManager::AudioDeviceSetup const setup{ filteredOutputDevice,   filteredInputDevice,
                                                            requestedSampleRate,    requestedBufferSize,
                                                            NEEDED_INPUT_CHANNELS,  false,
                                                            NEEDED_OUTPUT_CHANNELS, false };
    auto const errorString{ mAudioDeviceManager.setAudioDeviceSetup(setup, true) };
    if (errorString.isNotEmpty()) {
        return false;
    }
    return true;
}

//==============================================================================
void AudioManager::init(juce::String const & deviceType,
                        juce::String const & inputDevice,
                        juce::String const & outputDevice,
                        double const sampleRate,
                        int const bufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mInstance.reset(new AudioManager{ deviceType, inputDevice, outputDevice, sampleRate, bufferSize });
}

//==============================================================================
AudioManager::~AudioManager()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    if (mIsRecording) {
        stopRecording();
        mRecorders.clear(true);
    }
}

//==============================================================================
void AudioManager::registerAudioProcessor(AudioProcessor * audioProcessor)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mAudioProcessor = audioProcessor;
}

//==============================================================================
juce::StringArray AudioManager::getAvailableDeviceTypeNames()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::StringArray result{};
    for (auto const * deviceType : mAudioDeviceManager.getAvailableDeviceTypes()) {
        result.add(deviceType->getTypeName());
    }
    return result;
}

//==============================================================================
bool AudioManager::prepareToRecord(RecordingParameters const & recordingParams)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    static constexpr auto BITS_PER_SAMPLE = 24;
    static constexpr auto RECORD_QUALITY = 0;

    jassert(std::is_sorted(recordingParams.speakersToRecord.begin(), recordingParams.speakersToRecord.end()));
    mNumSamplesRecorded = 0;
    mRecorders.clearQuick(true);

    auto * currentAudioDevice{ mAudioDeviceManager.getCurrentAudioDevice() };
    jassert(currentAudioDevice);
    if (!currentAudioDevice) {
        return false;
    }

    static auto const GET_AUDIO_FORMAT = [](RecordingFormat const format) -> std::unique_ptr<juce::AudioFormat> {
        std::unique_ptr<juce::AudioFormat> audioFormat{};
        switch (format) {
        case RecordingFormat::aiff:
            return std::make_unique<juce::AiffAudioFormat>();
        case RecordingFormat::wav:
            return std::make_unique<juce::WavAudioFormat>();
#ifdef USE_CAF
        case RecordingFormat::caf:
            return std::make_unique<juce::CoreAudioFormat>();
#endif
        }
        return nullptr;
    };

    // prepare audioFormat
    auto const audioFormat{ GET_AUDIO_FORMAT(recordingParams.options.format) };
    if (!audioFormat) {
        jassertfalse;
        return false;
    }

    // subroutine to build the RecorderInfos
    static auto const makeRecordingInfo
        = [](juce::String const & path,
             juce::AudioFormat & format,
             double const sampleRate_,
             int const bufferSize_,
             juce::Array<float const *> dataToRecord,
             juce::TimeSliceThread & timeSlicedThread) -> std::unique_ptr<RecorderInfo> {
        juce::StringPairArray const metaData{}; // lets leave this empty for now

        juce::File const outputFile{ path };
        auto outputStream{ outputFile.createOutputStream() };
        jassert(outputStream);
        if (!outputStream) {
            return nullptr;
        }
        auto * audioFormatWriter{ format.createWriterFor(outputStream.release(),
                                                         sampleRate_,
                                                         dataToRecord.size(),
                                                         BITS_PER_SAMPLE,
                                                         metaData,
                                                         RECORD_QUALITY) };
        jassert(audioFormatWriter);
        if (!audioFormatWriter) {
            return nullptr;
        }
        auto threadedWriter{
            std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(audioFormatWriter, timeSlicedThread, bufferSize_)
        };
        jassert(threadedWriter);
        auto result{ std::make_unique<RecorderInfo>() };
        result->audioFormatWriter = audioFormatWriter;
        result->threadedWriter = std::move(threadedWriter);
        result->dataToRecord = std::move(dataToRecord);
        return result;
    };

    // Compute file paths
    auto const baseOutputFile{ juce::File{ recordingParams.path }.getParentDirectory().getFullPathName() + '/'
                               + juce::File{ recordingParams.path }.getFileNameWithoutExtension() };
    auto const extension{ juce::File{ recordingParams.path }.getFileExtension() };

    auto const getSeparateStereoFilePaths = [&]() {
        juce::StringArray result{};
        result.add(baseOutputFile + "L" + extension);
        result.add(baseOutputFile + "R" + extension);
        return result;
    };
    auto const getSeparateSpeakersFilePath = [&]() {
        juce::StringArray result{};
        result.ensureStorageAllocated(recordingParams.speakersToRecord.size());
        for (auto const outputPatch : recordingParams.speakersToRecord) {
            auto const path{ baseOutputFile + juce::String{ outputPatch.get() } + extension };
            jassert(!result.contains(path));
            result.add(path);
        }
        return result;
    };
    auto const getFilePaths = [&]() {
        if (recordingParams.options.fileType == RecordingFileType::interleaved) {
            return juce::StringArray{ baseOutputFile + extension };
        }

        jassert(recordingParams.options.fileType == RecordingFileType::mono);
        if (mStereoRouting) {
            return getSeparateStereoFilePaths();
        }

        return getSeparateSpeakersFilePath();
    };

    auto const filePaths{ getFilePaths() };

    // Delete files if needed
    auto deleteAllFiles{ false };
    for (auto const & fileName : filePaths) {
        juce::File file{ fileName };
        if (file.existsAsFile()) {
            if (!deleteAllFiles) {
                juce::AlertWindow alertWindow{ "error",
                                               "File \"" + file.getFullPathName() + "\" will be deleted. Proceed ?",
                                               juce::AlertWindow::WarningIcon };
                alertWindow.addButton("No", 0);
                alertWindow.addButton("yes", 1);
                alertWindow.addButton("Yes for all", 2);
                auto const result{ alertWindow.runModalLoop() };
                if (result == 0) {
                    return false;
                }
                if (result == 2) {
                    deleteAllFiles = true;
                }
            }
            auto const success{ file.deleteFile() };
            if (!success) {
                juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon,
                                                  "Error",
                                                  juce::String{ "Unable to overwrite file \"" } + file.getFullPathName()
                                                      + "\".");
                return false;
            }
        }
    }

    auto const recordingBufferSize{ RECORDERS_BUFFER_SIZE_IN_SAMPLES * recordingParams.speakersToRecord.size() };

    auto const makeInterleavedStereoRecorder = [&]() {
        jassert(filePaths.size() == 1);
        jassert(mStereoOutputBuffer.getNumChannels() == 2);
        juce::Array<float const *> dataToRecord{};
        dataToRecord.add(mStereoOutputBuffer.getReadPointer(0));
        dataToRecord.add(mStereoOutputBuffer.getReadPointer(1));
        auto recorder{ makeRecordingInfo(filePaths[0],
                                         *audioFormat,
                                         recordingParams.sampleRate,
                                         recordingBufferSize,
                                         std::move(dataToRecord),
                                         mRecordersThread) };
        if (!recorder) {
            return false;
        }
        mRecorders.add(std::move(recorder));
        return true;
    };
    auto const makeSeparateStereoRecorder = [&]() {
        jassert(filePaths.size() == 2);
        jassert(mStereoOutputBuffer.getNumChannels() == 2);
        for (int i{}; i < 2; ++i) {
            auto recorder{ makeRecordingInfo(filePaths[i],
                                             *audioFormat,
                                             recordingParams.sampleRate,
                                             recordingBufferSize,
                                             juce::Array<float const *>{ mStereoOutputBuffer.getReadPointer(i) },
                                             mRecordersThread) };
            if (!recorder) {
                return false;
            }
            mRecorders.add(std::move(recorder));
        }
        return true;
    };
    auto const makeInterleavedSpeakersRecorder = [&]() {
        jassert(filePaths.size() == 1);
        auto const & filePath{ filePaths[0] };
        auto dataToRecord{ mOutputBuffer.getArrayOfReadPointers(recordingParams.speakersToRecord) };
        auto recordingInfo{ makeRecordingInfo(filePath,
                                              *audioFormat,
                                              recordingParams.sampleRate,
                                              recordingBufferSize,
                                              std::move(dataToRecord),
                                              mRecordersThread) };
        if (!recordingInfo) {
            return false;
        }
        mRecorders.add(std::move(recordingInfo));
        return true;
    };
    auto const makeSeparateSpeakersRecorder = [&]() {
        jassert(recordingParams.speakersToRecord.size() == filePaths.size());
        for (int i{}; i < recordingParams.speakersToRecord.size(); ++i) {
            auto const outputPatch{ recordingParams.speakersToRecord[i] };
            auto const & filePath{ filePaths[i] };
            juce::Array<float const *> dataToRecord{};
            dataToRecord.add(mOutputBuffer[outputPatch].getReadPointer(0));
            auto recordingInfo{ makeRecordingInfo(filePath,
                                                  *audioFormat,
                                                  recordingParams.sampleRate,
                                                  recordingBufferSize,
                                                  std::move(dataToRecord),
                                                  mRecordersThread) };
            if (!recordingInfo) {
                return false;
            }
            mRecorders.add(std::move(recordingInfo));
        }
        return true;
    };

    auto const makeRecorders = [&]() {
        auto const isInterleaved{ recordingParams.options.fileType == RecordingFileType::interleaved };
        if (mStereoRouting) {
            if (isInterleaved) {
                return makeInterleavedStereoRecorder();
            }
            return makeSeparateStereoRecorder();
        }

        if (isInterleaved) {
            return makeInterleavedSpeakersRecorder();
        }

        return makeSeparateSpeakersRecorder();
    };

    // Make recorders
    auto const success{ makeRecorders() };
    if (!success) {
        jassertfalse;
        return false;
    }

    mRecordersThread.startThread();

    return true;
}

//==============================================================================
void AudioManager::startRecording()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mIsRecording = true;
}

//==============================================================================
void AudioManager::stopRecording()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(mAudioProcessor);
    juce::ScopedLock const sl{ mAudioProcessor->getLock() };
    // threadedWriters will flush their data before going off
    mRecorders.clear(true);
    mRecordersThread.stopThread(-1);
    mIsRecording = false;
}

//==============================================================================
void AudioManager::initInputBuffer(juce::Array<source_index_t> const & sources)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(mAudioProcessor);
    juce::ScopedLock const lock{ mAudioProcessor->getLock() };
    mInputBuffer.init(sources);
}

//==============================================================================
void AudioManager::initOutputBuffer(juce::Array<output_patch_t> const & speakers)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(mAudioProcessor);
    juce::ScopedLock const lock{ mAudioProcessor->getLock() };
    mOutputBuffer.init(speakers);
}

//==============================================================================
void AudioManager::setBufferSize(int const newBufferSize)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(mAudioProcessor);
    juce::ScopedLock const lock{ mAudioProcessor->getLock() };
    mInputBuffer.setNumSamples(newBufferSize);
    mOutputBuffer.setNumSamples(newBufferSize);
    mStereoOutputBuffer.setSize(2, newBufferSize);
}

//==============================================================================
void AudioManager::setStereoRouting(tl::optional<StereoRouting> const & stereoRouting)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedLock const lock{ mAudioProcessor->getLock() };
    mStereoRouting = stereoRouting;
}

//==============================================================================
void AudioManager::audioDeviceError(juce::String const & /*errorMessage*/)
{
    jassertfalse;
}

//==============================================================================
void AudioManager::audioDeviceAboutToStart(juce::AudioIODevice * /*device*/)
{
    // when AudioProcessor will be a real AudioSource, prepareToPlay() should be called here.
}

//==============================================================================
void AudioManager::audioDeviceStopped()
{
    // when AudioProcessor will be a real AudioSource, releaseResources() should be called here.
}

//==============================================================================
AudioManager & AudioManager::getInstance()
{
    jassert(mInstance);
    return *mInstance;
}

//==============================================================================
void AudioManager::free()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mInstance.reset();
}
