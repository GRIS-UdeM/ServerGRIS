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

#include "AudioManager.hpp"

class MainContentComponent;
class GrisLookAndFeel;

//==============================================================================
class PrepareToRecordComponent final
    : public juce::Component
    , public juce::TextButton::Listener
{
    static constexpr auto DEFAULT_FILE_NAME = "recording";

    MainContentComponent & mMainContentComponent;
    GrisLookAndFeel & mLookAndFeel;

    juce::TextEditor mPathEditor{};
    juce::TextButton mBrowseButton{};

    juce::TextButton mWavButton{};
    juce::TextButton mAiffButton{};
#ifdef USE_CAF
    juce::TextButton mCafButton{};
#endif

    juce::TextButton mMonoButton{};
    juce::TextButton mInterleavedButton{};
    juce::TextButton mRecordButton{};

public:
    //==============================================================================
    PrepareToRecordComponent(juce::File const & recordingDirectory,
                             RecordingOptions const & recordingOptions,
                             MainContentComponent & mainContentComponent,
                             GrisLookAndFeel & lookAndFeel);
    ~PrepareToRecordComponent() override = default;
    //==============================================================================
    PrepareToRecordComponent(PrepareToRecordComponent const &) = delete;
    PrepareToRecordComponent(PrepareToRecordComponent &&) = delete;
    PrepareToRecordComponent & operator=(PrepareToRecordComponent const &) = delete;
    PrepareToRecordComponent & operator=(PrepareToRecordComponent &&) = delete;
    //==============================================================================
    void resized() override;
    void buttonClicked(juce::Button * button) override;

private:
    //==============================================================================
    void performBrowse();
    void performRecord();
    bool isFileFormatButton(juce::Button const * button) const noexcept;
    void adjustPathExtension();

    RecordingFileType getSelectedFileType() const;
    RecordingFormat getSelectedFormat() const;
    //==============================================================================
    JUCE_LEAK_DETECTOR(PrepareToRecordComponent)
};

//==============================================================================
class PrepareToRecordWindow final : public juce::DocumentWindow
{
    MainContentComponent & mMainContentComponent;
    PrepareToRecordComponent mContentComponent;

public:
    //==============================================================================
    PrepareToRecordWindow(juce::File const & recordingDirectory,
                          RecordingOptions const & recordingOptions,
                          MainContentComponent & mainContentComponent,
                          GrisLookAndFeel & lookAndFeel);
    ~PrepareToRecordWindow() override = default;
    //==============================================================================
    PrepareToRecordWindow(PrepareToRecordWindow const &) = delete;
    PrepareToRecordWindow(PrepareToRecordWindow &&) = delete;
    PrepareToRecordWindow & operator=(PrepareToRecordWindow const &) = delete;
    PrepareToRecordWindow & operator=(PrepareToRecordWindow &&) = delete;
    //==============================================================================
    void closeButtonPressed() override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(PrepareToRecordWindow)
};