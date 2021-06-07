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

#include "PrepareToRecordWindow.hpp"

#include "GrisLookAndFeel.hpp"
#include "MainComponent.hpp"

static constexpr int PADDING = 10;
static constexpr auto BUTTONS_WIDTH = 100;
static constexpr auto BUTTONS_HEIGHT = 30;
static constexpr auto WIDTH = 600;
static constexpr auto NUM_ROWS = 3;
static constexpr auto HEIGHT = PADDING * (NUM_ROWS + 1) + BUTTONS_HEIGHT * NUM_ROWS;

using flags = juce::FileBrowserComponent::FileChooserFlags;

//==============================================================================
PrepareToRecordComponent::PrepareToRecordComponent(juce::File const & recordingDirectory,
                                                   RecordingOptions const & recordingOptions,
                                                   MainContentComponent & mainContentComponent,
                                                   GrisLookAndFeel & lookAndFeel)
    : mMainContentComponent(mainContentComponent)
    , mLookAndFeel(lookAndFeel)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const validDirectory{ recordingDirectory.isDirectory()
                                   ? recordingDirectory
                                   : juce::File::getSpecialLocation(
                                       juce::File::SpecialLocationType::userDesktopDirectory) };
    auto const path{ validDirectory.getChildFile(DEFAULT_FILE_NAME) };
    mPathEditor.setColour(juce::TextEditor::ColourIds::backgroundColourId, juce::Colours::transparentBlack);
    mPathEditor.setColour(juce::TextEditor::ColourIds::outlineColourId, juce::Colours::white);
    mPathEditor.setBorder(juce::BorderSize<int>{ 1 });
    mPathEditor.setText(path.getFullPathName());
    mPathEditor.setJustification(juce::Justification::centredRight);
    mPathEditor.setScrollbarsShown(true);
    mPathEditor.onFocusLost = [&]() { mPathEditor.setCaretPosition(mPathEditor.getText().length()); };
    addAndMakeVisible(mPathEditor);

    mBrowseButton.setButtonText("Browse");
    mBrowseButton.addListener(this);
    addAndMakeVisible(mBrowseButton);

    auto const initFormatButton = [&](juce::TextButton & button, RecordingFormat const format) {
        button.setButtonText(recordingFormatToString(format));
        button.setClickingTogglesState(true);
        button.setRadioGroupId(PREPARE_TO_RECORD_WINDOW_FILE_FORMAT_GROUP_ID, juce::dontSendNotification);
        button.addListener(this);
        addAndMakeVisible(button);
    };

    initFormatButton(mWavButton, RecordingFormat::wav);
    initFormatButton(mAiffButton, RecordingFormat::aiff);
#ifdef USE_CAF
    initFormatButton(mCafButton, RecordingFormat::caf);
#endif

    auto const initTypeButton = [&](juce::TextButton & button, RecordingFileType const type) {
        button.setButtonText(recordingFileTypeToString(type));
        button.setClickingTogglesState(true);
        button.setRadioGroupId(PREPARE_TO_RECORD_WINDOW_FILE_TYPE_GROUP_ID, juce::dontSendNotification);
        addAndMakeVisible(button);
    };

    initTypeButton(mMonoButton, RecordingFileType::mono);
    initTypeButton(mInterleavedButton, RecordingFileType::interleaved);

    mRecordButton.setButtonText("Record");
    mRecordButton.addListener(this);
    mRecordButton.setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colours::red.withSaturation(0.5f));
    addAndMakeVisible(mRecordButton);

    switch (recordingOptions.format) {
    case RecordingFormat::wav:
        mWavButton.setToggleState(true, juce::dontSendNotification);
        break;
    case RecordingFormat::aiff:
        mAiffButton.setToggleState(true, juce::dontSendNotification);
        break;
#ifdef USE_CAF
    case RecordingFormat::caf:
        mCafButton.setToggleState(true, juce::dontSendNotification);
        break;
#endif
    default:
        jassertfalse;
    }

    switch (recordingOptions.fileType) {
    case RecordingFileType::mono:
        mMonoButton.setToggleState(true, juce::dontSendNotification);
        break;
    case RecordingFileType::interleaved:
        mInterleavedButton.setToggleState(true, juce::dontSendNotification);
        break;
    default:
        jassertfalse;
    }

    adjustPathExtension();
}

//==============================================================================
void PrepareToRecordComponent::resized()
{
    auto yOffset{ PADDING };
    auto xOffset{ PADDING };

    auto const nextLine = [&]() { yOffset += PADDING + BUTTONS_HEIGHT; };
    auto const nextButton = [&]() { xOffset += BUTTONS_WIDTH; };
    auto const resetX = [&]() { xOffset = PADDING; };

    auto const pathWidth{ WIDTH - PADDING * 3 - BUTTONS_WIDTH };
    auto const pathBounds{ juce::Rectangle<int>{ PADDING, PADDING, pathWidth, BUTTONS_HEIGHT }.reduced(0, 1) };
    mPathEditor.setBounds(pathBounds);
    mBrowseButton.setBounds(PADDING * 2 + pathWidth, PADDING, BUTTONS_WIDTH, BUTTONS_HEIGHT);

    nextLine();

    mWavButton.setBounds(xOffset, yOffset, BUTTONS_WIDTH, BUTTONS_HEIGHT);
    nextButton();
    mAiffButton.setBounds(xOffset, yOffset, BUTTONS_WIDTH, BUTTONS_HEIGHT);
#ifdef USE_CAF
    nextButton();
    mCafButton.setBounds(xOffset, yOffset, BUTTONS_WIDTH, BUTTONS_HEIGHT);
#endif

    resetX();
    nextLine();

    mMonoButton.setBounds(xOffset, yOffset, BUTTONS_WIDTH, BUTTONS_HEIGHT);
    nextButton();
    mInterleavedButton.setBounds(xOffset, yOffset, BUTTONS_WIDTH, BUTTONS_HEIGHT);
    mRecordButton.setBounds(WIDTH - PADDING - BUTTONS_WIDTH, yOffset, BUTTONS_WIDTH, BUTTONS_HEIGHT);
}

//==============================================================================
void PrepareToRecordComponent::buttonClicked(juce::Button * button)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (button == &mBrowseButton) {
        performBrowse();
        return;
    }

    if (button == &mRecordButton) {
        performRecord();
        return;
    }

    if (isFileFormatButton(button)) {
        if (!button->getToggleState()) {
            return;
        }
        adjustPathExtension();
    }
}

//==============================================================================
void PrepareToRecordComponent::performBrowse()
{
    auto const extension{ "*." + recordingFormatToString(getSelectedFormat()).toLowerCase() };

    juce::FileChooser fileChooser{ "SpatGris recording", mPathEditor.getText(), extension, true, false, this };
    if (!fileChooser.browseForFileToSave(false)) {
        return;
    }
    auto const file{ fileChooser.getResult() };

    mPathEditor.setText(file.getFullPathName(), juce::dontSendNotification);
    adjustPathExtension();
}

//==============================================================================
void PrepareToRecordComponent::performRecord()
{
    adjustPathExtension();

    auto const fileType{ getSelectedFileType() };
    auto const format{ getSelectedFormat() };

    juce::File const path{ mPathEditor.getText() };

    auto const getFinalPath = [&]() {
        auto const expectedExtension{ "." + recordingFormatToString(format).toLowerCase() };

        if (path.getFullPathName().endsWithIgnoreCase(expectedExtension)) {
            return path;
        }
        return juce::File{ path.getFullPathName() + expectedExtension };
    };

    auto const finalPath{ getFinalPath() };

    if (!finalPath.getParentDirectory().isDirectory()) {
        auto const message{ juce::String{ "The parent directory \"" } + finalPath.getParentDirectory().getFullPathName()
                            + "\" does not exist." };
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::AlertIconType::WarningIcon,
                                               "Error",
                                               message,
                                               "Ok",
                                               this);
        return;
    }

    RecordingOptions const recordingOptions{ format, fileType };
    mMainContentComponent.prepareAndStartRecording(finalPath, recordingOptions);
}

//==============================================================================
bool PrepareToRecordComponent::isFileFormatButton(juce::Button const * button) const noexcept
{
    if (button == &mWavButton || button == &mAiffButton) {
        return true;
    }
#ifdef USE_CAF
    if (button == &mCafButton) {
        return true;
    }
#endif
    return false;
}

//==============================================================================
void PrepareToRecordComponent::adjustPathExtension()
{
    auto const newFormat{ getSelectedFormat() };
    auto const newExtension{ "." + recordingFormatToString(newFormat).toLowerCase() };
    auto const currentPath{ mPathEditor.getText() };

    for (auto const & possibleExtension : RECORDING_FORMAT_STRINGS) {
        auto const extensionToTest{ "." + possibleExtension.toLowerCase() };
        if (currentPath.endsWithIgnoreCase(extensionToTest)) {
            mPathEditor.setText(currentPath.upToLastOccurrenceOf(extensionToTest, false, true) + newExtension);
            return;
        }
    }

    mPathEditor.setText(currentPath + newExtension);
}

//==============================================================================
RecordingFileType PrepareToRecordComponent::getSelectedFileType() const
{
    if (mMonoButton.getToggleState()) {
        return RecordingFileType::mono;
    }
    jassert(mInterleavedButton.getToggleState());
    return RecordingFileType::interleaved;
}

//==============================================================================
RecordingFormat PrepareToRecordComponent::getSelectedFormat() const
{
    if (mWavButton.getToggleState()) {
        return RecordingFormat::wav;
    }
    if (mAiffButton.getToggleState()) {
        return RecordingFormat::aiff;
    }
#ifdef USE_CAF
    jassert(mCafButton.getToggleState());
    return RecordingFormat::caf;
#else
    jassertfalse;
    return RecordingFormat::wav;
#endif
}

//==============================================================================
PrepareToRecordWindow::PrepareToRecordWindow(juce::File const & recordingDirectory,
                                             RecordingOptions const & recordingOptions,
                                             MainContentComponent & mainContentComponent,
                                             GrisLookAndFeel & lookAndFeel)
    : DocumentWindow("Start recording", lookAndFeel.getBackgroundColour(), DocumentWindow::closeButton)
    , mMainContentComponent(mainContentComponent)
    , mContentComponent(recordingDirectory, recordingOptions, mainContentComponent, lookAndFeel)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    setUsingNativeTitleBar(true);
    setContentNonOwned(&mContentComponent, false);
    centreAroundComponent(&mainContentComponent, WIDTH, HEIGHT);
    mContentComponent.setSize(WIDTH, HEIGHT);
    DocumentWindow::setVisible(true);
}

//==============================================================================
void PrepareToRecordWindow::closeButtonPressed()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mMainContentComponent.closePrepareToRecordWindow();
}
