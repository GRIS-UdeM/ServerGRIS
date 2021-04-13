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

#include "MainComponent.h"

#include "AudioManager.h"
#include "MainWindow.h"
#include "VuMeterComponent.h"
#include "constants.hpp"

//==============================================================================
MainContentComponent::MainContentComponent(MainWindow & mainWindow,
                                           GrisLookAndFeel & grisLookAndFeel,
                                           SmallGrisLookAndFeel & smallGrisLookAndFeel)
    : mLookAndFeel(grisLookAndFeel)
    , mSmallLookAndFeel(smallGrisLookAndFeel)
    , mMainWindow(mainWindow)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&grisLookAndFeel);

    mData.appData = mConfiguration.load();

    // init audio
    // TODO: probably a race condition here
    auto const & audioSettings{ mData.appData.audioSettings };
    AudioManager::init(audioSettings.deviceType,
                       audioSettings.inputDevice,
                       audioSettings.outputDevice,
                       audioSettings.sampleRate,
                       audioSettings.bufferSize);
    mAudioProcessor = std::make_unique<AudioProcessor>();

    juce::ScopedLock const audioLock{ mAudioProcessor->getCriticalSection() };

    auto & audioManager{ AudioManager::getInstance() };
    audioManager.registerAudioProcessor(mAudioProcessor.get());

    mAudioProcessor->setAudioConfig(mData.toAudioConfig());

    // Create the menu bar.
    mMenuBar.reset(new juce::MenuBarComponent(this));
    addAndMakeVisible(mMenuBar.get());

    // SpeakerViewComponent 3D view
    mSpeakerViewComponent.reset(new SpeakerViewComponent(*this));
    addAndMakeVisible(mSpeakerViewComponent.get());

    // Box Main
    mMainUiBox.reset(new Box(mLookAndFeel, "", true, false));
    addAndMakeVisible(mMainUiBox.get());

    // Box Inputs
    mInputsUiBox.reset(new Box(mLookAndFeel, "Inputs"));
    addAndMakeVisible(mInputsUiBox.get());

    // Box Outputs
    mOutputsUiBox.reset(new Box(mLookAndFeel, "Outputs"));
    addAndMakeVisible(mOutputsUiBox.get());

    // Box Control
    mControlUiBox.reset(new Box(mLookAndFeel, "Controls"));
    addAndMakeVisible(mControlUiBox.get());

    mMainUiBox->getContent()->addAndMakeVisible(mInputsUiBox.get());
    mMainUiBox->getContent()->addAndMakeVisible(mOutputsUiBox.get());
    mMainUiBox->getContent()->addAndMakeVisible(mControlUiBox.get());

    // Components in Box Control
    mCpuUsageLabel.reset(addLabel("CPU usage", "CPU usage", 0, 0, 80, 28, mControlUiBox->getContent()));
    mCpuUsageValue.reset(addLabel("0 %", "CPU usage", 80, 0, 80, 28, mControlUiBox->getContent()));
    mSampleRateLabel.reset(addLabel("0 Hz", "Rate", 120, 0, 80, 28, mControlUiBox->getContent()));
    mBufferSizeLabel.reset(addLabel("0 spls", "Buffer Size", 200, 0, 80, 28, mControlUiBox->getContent()));
    mChannelCountLabel.reset(addLabel("...", "Inputs/Outputs", 280, 0, 90, 28, mControlUiBox->getContent()));

    mCpuUsageLabel->setColour(juce::Label::backgroundColourId, mLookAndFeel.getWinBackgroundColour());
    mCpuUsageValue->setColour(juce::Label::backgroundColourId, mLookAndFeel.getWinBackgroundColour());
    mSampleRateLabel->setColour(juce::Label::backgroundColourId, mLookAndFeel.getWinBackgroundColour());
    mBufferSizeLabel->setColour(juce::Label::backgroundColourId, mLookAndFeel.getWinBackgroundColour());
    mChannelCountLabel->setColour(juce::Label::backgroundColourId, mLookAndFeel.getWinBackgroundColour());

    addLabel("Gain", "Master Gain Outputs", 15, 30, 120, 20, mControlUiBox->getContent());
    mMasterGainOutSlider.reset(
        addSlider("Master Gain", "Master Gain Outputs", 5, 45, 60, 60, mControlUiBox->getContent()));
    mMasterGainOutSlider->setRange(-60.0, 12.0, 0.01);
    mMasterGainOutSlider->setTextValueSuffix(" dB");

    addLabel("Interpolation", "Master Interpolation", 60, 30, 120, 20, mControlUiBox->getContent());
    mInterpolationSlider.reset(addSlider("Inter", "Interpolation", 70, 45, 60, 60, mControlUiBox->getContent()));
    mInterpolationSlider->setRange(0.0, 1.0, 0.001);

    addLabel("Mode :", "Mode of spatialization", 150, 30, 60, 20, mControlUiBox->getContent());
    mSpatModeCombo.reset(addComboBox("", "Mode of spatialization", 155, 48, 90, 22, mControlUiBox->getContent()));
    for (int i{}; i < MODE_SPAT_STRING.size(); i++) {
        mSpatModeCombo->addItem(MODE_SPAT_STRING[i], i + 1);
    }

    mNumSourcesTextEditor.reset(
        addTextEditor("Inputs :", "0", "Numbers of Inputs", 122, 83, 43, 22, mControlUiBox->getContent()));
    mNumSourcesTextEditor->setInputRestrictions(3, "0123456789");

    mInitRecordButton.reset(
        addButton("Init Recording", "Init Recording", 268, 48, 103, 24, mControlUiBox->getContent()));

    mStartRecordButton.reset(addButton("Record", "Start/Stop Record", 268, 83, 60, 24, mControlUiBox->getContent()));
    mStartRecordButton->setEnabled(false);

    mTimeRecordedLabel.reset(addLabel("00:00", "Record time", 327, 83, 50, 24, mControlUiBox->getContent()));

    // Set up the layout and resize bars.
    mVerticalLayout.setItemLayout(0,
                                  -0.2,
                                  -0.8,
                                  -0.435);     // width of the speaker view must be between 20% and 80%, preferably 50%
    mVerticalLayout.setItemLayout(1, 8, 8, 8); // the vertical divider drag-bar thing is always 8 pixels wide
    mVerticalLayout.setItemLayout(
        2,
        150.0,
        -1.0,
        -0.565); // right panes must be at least 150 pixels wide, preferably 50% of the total width
    mVerticalDividerBar.reset(new juce::StretchableLayoutResizerBar(&mVerticalLayout, 1, true));
    addAndMakeVisible(mVerticalDividerBar.get());

    // Default application window size.
    setSize(1285, 610);

    jassert(AudioManager::getInstance().getAudioDeviceManager().getCurrentAudioDevice());

    mCpuUsageLabel->setText("CPU usage : ", juce::dontSendNotification);

    AudioManager::getInstance().getAudioDeviceManager().addChangeListener(this);
    audioParametersChanged();

    // Start the OSC Receiver.
    mOscReceiver.reset(new OscInput(*this));
    mOscReceiver->startConnection(mData.project.oscPort);

    // Default widget values.
    mMasterGainOutSlider->setValue(0.0);
    mInterpolationSlider->setValue(0.1);
    mSpatModeCombo->setSelectedId(1);

    mNumSourcesTextEditor->setText("16", juce::dontSendNotification);
    textEditorReturnKeyPressed(*mNumSourcesTextEditor);

    // Open the default project if lastOpenProject is not a valid file.
    openProject(mData.appData.lastProject);

    // Open the default speaker setup if lastOpenSpeakerSetup is not a valid file.
    auto const lastSpatMode{ mData.appData.spatMode };
    switch (lastSpatMode) {
    case SpatMode::hrtfVbap:
        loadSpeakerSetup(BINAURAL_SPEAKER_SETUP_FILE, lastSpatMode);
        break;
    case SpatMode::lbap:
    case SpatMode::vbap:
        loadSpeakerSetup(mData.appData.lastSpeakerSetup, lastSpatMode);
        break;
    case SpatMode::stereo:
        loadSpeakerSetup(STEREO_SPEAKER_SETUP_FILE, lastSpatMode);
        break;
    }

    // End layout and start refresh timer.
    resized();
    startTimerHz(24);

    // Start Splash screen.
#if NDEBUG
    if (SPLASH_SCREEN_FILE.exists()) {
        mSplashScreen.reset(
            new juce::SplashScreen("SpatGRIS3", juce::ImageFileFormat::loadFrom(SPLASH_SCREEN_FILE), true));
        mSplashScreen->deleteAfterDelay(juce::RelativeTime::seconds(4), false);
        mSplashScreen.release();
    }
#endif

    // Initialize the command manager for the menu bar items.
    auto & commandManager{ mMainWindow.getApplicationCommandManager() };
    commandManager.registerAllCommandsForTarget(this);

    // Restore last vertical divider position and speaker view cam distance.
    auto const sashPosition{ mData.appData.sashPosition };
    auto const trueSize{ narrow<int>(std::round(narrow<double>(getWidth() - 3) * std::abs(sashPosition))) };
    mVerticalLayout.setItemPosition(1, trueSize);

    mSpatAlgorithm = AbstractSpatAlgorithm::make(lastSpatMode);
    mSpatAlgorithm->init(mData.speakerSetup.speakers);
}

//==============================================================================
MainContentComponent::~MainContentComponent()
{
    mData.appData.sashPosition = mVerticalLayout.getItemCurrentRelativeSize(0);
    // TODO : save other appData thingies

    mConfiguration.save(mData.appData);

    mSpeakerViewComponent.reset();

    juce::ScopedLock const lock{ mCriticalSection };
    mSpeakerModels.clear();
    mSourceModels.clear();
}

//==============================================================================
// Widget builder utilities.
juce::Label * MainContentComponent::addLabel(juce::String const & s,
                                             juce::String const & tooltip,
                                             int const x,
                                             int const y,
                                             int const w,
                                             int const h,
                                             Component * into) const
{
    auto * lb{ new juce::Label{} };
    lb->setText(s, juce::NotificationType::dontSendNotification);
    lb->setTooltip(tooltip);
    lb->setJustificationType(juce::Justification::left);
    lb->setFont(mLookAndFeel.getFont());
    lb->setLookAndFeel(&mLookAndFeel);
    lb->setColour(juce::Label::textColourId, mLookAndFeel.getFontColour());
    lb->setBounds(x, y, w, h);
    into->addAndMakeVisible(lb);
    return lb;
}

//==============================================================================
juce::TextButton * MainContentComponent::addButton(juce::String const & s,
                                                   juce::String const & tooltip,
                                                   int const x,
                                                   int const y,
                                                   int const w,
                                                   int const h,
                                                   Component * into)
{
    auto * tb{ new juce::TextButton{} };
    tb->setTooltip(tooltip);
    tb->setButtonText(s);
    tb->setSize(w, h);
    tb->setTopLeftPosition(x, y);
    tb->addListener(this);
    tb->setColour(juce::ToggleButton::textColourId, mLookAndFeel.getFontColour());
    tb->setLookAndFeel(&mLookAndFeel);
    into->addAndMakeVisible(tb);
    return tb;
}

//==============================================================================
juce::ToggleButton * MainContentComponent::addToggleButton(juce::String const & s,
                                                           juce::String const & tooltip,
                                                           int const x,
                                                           int const y,
                                                           int const w,
                                                           int const h,
                                                           Component * into,
                                                           bool const toggle)
{
    auto * tb{ new juce::ToggleButton{} };
    tb->setTooltip(tooltip);
    tb->setButtonText(s);
    tb->setToggleState(toggle, juce::dontSendNotification);
    tb->setSize(w, h);
    tb->setTopLeftPosition(x, y);
    tb->addListener(this);
    tb->setColour(juce::ToggleButton::textColourId, mLookAndFeel.getFontColour());
    tb->setLookAndFeel(&mLookAndFeel);
    into->addAndMakeVisible(tb);
    return tb;
}

//==============================================================================
juce::TextEditor * MainContentComponent::addTextEditor(juce::String const & s,
                                                       juce::String const & emptyS,
                                                       juce::String const & tooltip,
                                                       int const x,
                                                       int const y,
                                                       int const w,
                                                       int const h,
                                                       Component * into,
                                                       int const wLab)
{
    auto * te{ new juce::TextEditor{} };
    te->setTooltip(tooltip);
    te->setTextToShowWhenEmpty(emptyS, mLookAndFeel.getOffColour());
    te->setColour(juce::ToggleButton::textColourId, mLookAndFeel.getFontColour());
    te->setLookAndFeel(&mLookAndFeel);

    if (s.isEmpty()) {
        te->setBounds(x, y, w, h);
    } else {
        te->setBounds(x + wLab, y, w, h);
        juce::Label * lb = addLabel(s, "", x, y, wLab, h, into);
        lb->setJustificationType(juce::Justification::centredRight);
    }

    te->addListener(this);
    into->addAndMakeVisible(te);
    return te;
}

//==============================================================================
juce::Slider * MainContentComponent::addSlider(juce::String const & /*s*/,
                                               juce::String const & tooltip,
                                               int const x,
                                               int const y,
                                               int const w,
                                               int const h,
                                               Component * into)
{
    auto * sd{ new juce::Slider{} };
    sd->setTooltip(tooltip);
    sd->setSize(w, h);
    sd->setTopLeftPosition(x, y);
    sd->setSliderStyle(juce::Slider::Rotary);
    sd->setRotaryParameters(juce::MathConstants<float>::pi * 1.3f, juce::MathConstants<float>::pi * 2.7f, true);
    sd->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    sd->setColour(juce::ToggleButton::textColourId, mLookAndFeel.getFontColour());
    sd->setLookAndFeel(&mLookAndFeel);
    sd->addListener(this);
    into->addAndMakeVisible(sd);
    return sd;
}

//==============================================================================
juce::ComboBox * MainContentComponent::addComboBox(juce::String const & /*s*/,
                                                   juce::String const & tooltip,
                                                   int const x,
                                                   int const y,
                                                   int const w,
                                                   int const h,
                                                   Component * into)
{
    // TODO : naked new
    auto * cb{ new juce::ComboBox{} };
    cb->setTooltip(tooltip);
    cb->setSize(w, h);
    cb->setTopLeftPosition(x, y);
    cb->setLookAndFeel(&mLookAndFeel);
    cb->addListener(this);
    into->addAndMakeVisible(cb);
    return cb;
}

//==============================================================================
juce::ApplicationCommandTarget * MainContentComponent::getNextCommandTarget()
{
    return findFirstTargetParentComponent();
}

//==============================================================================
// Menu item action handlers.
void MainContentComponent::handleNew()
{
    juce::AlertWindow alert{ "Closing current project !", "Do you want to save ?", juce::AlertWindow::InfoIcon };
    alert.setLookAndFeel(&mLookAndFeel);
    alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::deleteKey));
    alert.addButton("yes", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert.addButton("No", 2, juce::KeyPress(juce::KeyPress::escapeKey));

    auto const status{ alert.runModalLoop() };
    if (status == 1) {
        handleSaveProject();
    } else if (status == 0) {
        return;
    }

    openProject(DEFAULT_PROJECT_FILE.getFullPathName());
}

//==============================================================================
void MainContentComponent::openProject(juce::File const & file)
{
    jassert(file.existsAsFile());

    juce::XmlDocument xmlDoc{ file };
    auto const mainXmlElem{ xmlDoc.getDocumentElement() };

    if (!mainXmlElem) {
        // Formatting error
        juce::AlertWindow::showMessageBox(juce::AlertWindow::AlertIconType::WarningIcon,
                                          "Error in Open Project !",
                                          "Your file is corrupted !\n" + file.getFullPathName() + "\n"
                                              + xmlDoc.getLastParseError());
        return;
    }

    if (!mainXmlElem->hasTagName("SpatServerGRIS_Preset") && !mainXmlElem->hasTagName("ServerGRIS_Preset")) {
        // Wrong file type
        auto const msg{ mainXmlElem->hasTagName("SpeakerSetup")
                            ? "You are trying to open a Speaker Setup instead of a project file !"
                            : "Your file is corrupted !\n" + xmlDoc.getLastParseError() };
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Error in Open Project !", msg);
        return;
    }

    auto projectData{ SpatGrisProjectData::fromXml(*mainXmlElem) };
    if (!projectData) {
        // Missing params
        juce::AlertWindow::showMessageBox(juce::AlertWindow::AlertIconType::WarningIcon,
                                          "Unable to read project file !",
                                          "One or more mandatory parameters are missing !");
        return;
    }
    mData.project = std::move(*projectData);

    mNumSourcesTextEditor->setText(juce::String{ mData.project.sources.size() });

    mMasterGainOutSlider->setValue(mData.project.masterGain.get(), juce::dontSendNotification);
    mInterpolationSlider->setValue(mData.project.spatGainsInterpolation, juce::dontSendNotification);

    mSpeakerViewComponent->setShowNumber(mData.project.viewSettings.showSpeakerNumbers);
    mSpeakerViewComponent->setHideSpeaker(!mData.project.viewSettings.showSpeakers);
    mSpeakerViewComponent->setShowTriplets(mData.project.viewSettings.showSpeakerTriplets);
    mSpeakerViewComponent->setShowSphere(mData.project.viewSettings.showSphereOrCube);
    mSpeakerViewComponent->setCamPosition(mData.project.cameraPosition);
    // DEFAULT
    // mSpeakerViewComponent->setCamPosition(80.0f, 25.0f, 22.0f);

    refreshSourceVuMeterComponents();

    mData.appData.lastProject = file.getFullPathName();
    mAudioProcessor->setAudioConfig(mData.toAudioConfig());

    setTitle();
}

//==============================================================================
void MainContentComponent::handleOpenProject()
{
    juce::File const lastOpenProject{ mData.appData.lastProject };
    auto const dir{ lastOpenProject.getParentDirectory() };
    auto const filename{ lastOpenProject.getFileName() };

    juce::FileChooser fc("Choose a file to open...", dir.getFullPathName() + "/" + filename, "*.xml", true);

    auto loaded{ false };
    if (fc.browseForFileToOpen()) {
        auto const chosen{ fc.getResults().getReference(0).getFullPathName() };
        juce::AlertWindow alert("Open Project !",
                                "You want to load : " + chosen + "\nEverything not saved will be lost !",
                                juce::AlertWindow::WarningIcon);
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        alert.addButton("Ok", 1, juce::KeyPress(juce::KeyPress::returnKey));
        if (alert.runModalLoop() != 0) {
            openProject(chosen);
            loaded = true;
        }
    }

    if (loaded) { // Check for direct out OutputPatch mismatch.
        for (auto const source : mData.project.sources) {
            auto const & directOut{ source.value->directOut };
            if (directOut) {
                if (!mData.speakerSetup.speakers.contains(*directOut)) {
                    juce::AlertWindow alert(
                        "Direct Out Mismatch!",
                        "Some of the direct out channels of this project don't exist in the current speaker setup.\n",
                        juce::AlertWindow::WarningIcon);
                    alert.setLookAndFeel(&mLookAndFeel);
                    alert.addButton("Ok", 1, juce::KeyPress(juce::KeyPress::returnKey));
                    alert.runModalLoop();
                    break;
                    // TODO : reload last project?
                }
            }
        }
    }
}

//==============================================================================
void MainContentComponent::handleSaveProject()
{
    juce::File const lastOpenProject{ mData.appData.lastProject };
    if (!lastOpenProject.existsAsFile()
        || lastOpenProject.getFullPathName().endsWith("default_preset/default_preset.xml")) {
        handleSaveAsProject();
    }
    saveProject(lastOpenProject.getFullPathName());
}

//==============================================================================
void MainContentComponent::handleSaveAsProject()
{
    juce::File const lastOpenProject{ mData.appData.lastProject };

    juce::FileChooser fc{ "Choose a file to save...", lastOpenProject.getFullPathName(), "*.xml", true };

    if (fc.browseForFileToSave(true)) {
        auto const chosen{ fc.getResults().getReference(0).getFullPathName() };
        saveProject(chosen);
    }
}

//==============================================================================
void MainContentComponent::handleOpenSpeakerSetup()
{
    juce::FileChooser fc{ "Choose a file to open...", mCurrentSpeakerSetup, "*.xml", true };

    if (fc.browseForFileToOpen()) {
        auto const chosen{ fc.getResults().getReference(0).getFullPathName() };
        juce::AlertWindow alert{ "Load Speaker Setup !",
                                 "You want to load : " + chosen + "\nEverything not saved will be lost !",
                                 juce::AlertWindow::WarningIcon };
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        alert.addButton("Ok", 1, juce::KeyPress(juce::KeyPress::returnKey));
        if (alert.runModalLoop() != 0) {
            alert.setVisible(false);
            loadSpeakerSetup(chosen);
        }
    }
}

//==============================================================================
void MainContentComponent::handleSaveAsSpeakerSetup()
{
    juce::FileChooser fc{ "Choose a file to save...", mCurrentSpeakerSetup, "*.xml", true };

    if (fc.browseForFileToSave(true)) {
        auto const chosen{ fc.getResults().getReference(0).getFullPathName() };
        saveSpeakerSetup(chosen);
    }
}

//==============================================================================
void MainContentComponent::closeSpeakersConfigurationWindow()
{
    mNeedToSaveSpeakerSetup = false;
    mEditSpeakersWindow.reset();
}

//==============================================================================
void MainContentComponent::handleShowSpeakerEditWindow()
{
    juce::Rectangle<int> const result{ getScreenX() + mSpeakerViewComponent->getWidth() + 20,
                                       getScreenY() + 20,
                                       850,
                                       600 };
    if (mEditSpeakersWindow == nullptr) {
        auto const windowName = juce::String("Speakers Setup Edition - ")
                                + juce::String(MODE_SPAT_STRING[static_cast<int>(mData.appData.spatMode)]) + " - "
                                + mCurrentSpeakerSetup.getFileName();
        mEditSpeakersWindow = std::make_unique<EditSpeakersWindow>(windowName, mLookAndFeel, *this, mConfigurationName);
        mEditSpeakersWindow->setBounds(result);
        mEditSpeakersWindow->initComp();
    }
    mEditSpeakersWindow->setBounds(result);
    mEditSpeakersWindow->setResizable(true, true);
    mEditSpeakersWindow->setUsingNativeTitleBar(true);
    mEditSpeakersWindow->setVisible(true);
    mEditSpeakersWindow->setAlwaysOnTop(true);
    mEditSpeakersWindow->repaint();
}

//==============================================================================
void MainContentComponent::handleShowPreferences()
{
    if (mPropertiesWindow == nullptr) {
        mPropertiesWindow.reset(new SettingsWindow{ *this,
                                                    mData.appData.recordingOptions,
                                                    mData.project.lbapDistanceAttenuationData,
                                                    mData.project.oscPort,
                                                    mLookAndFeel });
    }
}

//==============================================================================
void MainContentComponent::handleShow2DView()
{
    if (mFlatViewWindow == nullptr) {
        mFlatViewWindow.reset(new FlatViewWindow{ *this, mLookAndFeel });
    } else {
        mFlatViewWindowRect.setBounds(mFlatViewWindow->getScreenX(),
                                      mFlatViewWindow->getScreenY(),
                                      mFlatViewWindow->getWidth(),
                                      mFlatViewWindow->getHeight());
    }

    if (mFlatViewWindowRect.getWidth() == 0) {
        mFlatViewWindowRect.setBounds(getScreenX() + mSpeakerViewComponent->getWidth() + 22,
                                      getScreenY() + 100,
                                      500,
                                      500);
    }

    mFlatViewWindow->setBounds(mFlatViewWindowRect);
    mFlatViewWindow->setResizable(true, true);
    mFlatViewWindow->setUsingNativeTitleBar(true);
    mFlatViewWindow->setVisible(true);
}

//==============================================================================
void MainContentComponent::handleShowOscLogView()
{
    if (mOscLogWindow == nullptr) {
        mOscLogWindow.reset(new OscLogWindow("OSC Logging Windows",
                                             mLookAndFeel.getWinBackgroundColour(),
                                             juce::DocumentWindow::allButtons,
                                             this,
                                             &mLookAndFeel));
    }
    mOscLogWindow->centreWithSize(500, 500);
    mOscLogWindow->setResizable(false, false);
    mOscLogWindow->setUsingNativeTitleBar(true);
    mOscLogWindow->setVisible(true);
    mOscLogWindow->repaint();
}

//==============================================================================
void MainContentComponent::handleShowAbout()
{
    if (!mAboutWindow) {
        mAboutWindow.reset(new AboutWindow{ "About SpatGRIS", mLookAndFeel, *this });
    }
}

//==============================================================================
void MainContentComponent::handleOpenManual()
{
    if (SERVER_GRIS_MANUAL_FILE.exists()) {
        juce::Process::openDocument("file:" + SERVER_GRIS_MANUAL_FILE.getFullPathName(), juce::String());
    }
}

//==============================================================================
void MainContentComponent::handleShowNumbers()
{
    auto & var{ mData.project.viewSettings.showSpeakerNumbers };
    var = !var;
    mSpeakerViewComponent->setShowNumber(var);
}

//==============================================================================
void MainContentComponent::handleShowSpeakers()
{
    auto & var{ mData.project.viewSettings.showSpeakers };
    var = !var;
    mSpeakerViewComponent->setHideSpeaker(!var);
}

//==============================================================================
void MainContentComponent::handleShowTriplets()
{
    auto const newState{ !mData.project.viewSettings.showSpeakerTriplets };
    if ((mData.appData.spatMode == SpatMode::lbap || mData.appData.spatMode == SpatMode::stereo) && newState) {
        juce::AlertWindow alert("Can't draw triplets !",
                                "Triplets are not effective with the CUBE or STEREO modes.",
                                juce::AlertWindow::InfoIcon);
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("Close", 0, juce::KeyPress(juce::KeyPress::returnKey));
        alert.runModalLoop();
        mSpeakerViewComponent->setShowTriplets(false);
        return;
    }

    /*if (!validateShowTriplets() && newState) {
        juce::AlertWindow alert("Can't draw all triplets !",
                                "Maybe you didn't compute your current speaker setup ?",
                                juce::AlertWindow::InfoIcon);
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("Close", 0, juce::KeyPress(juce::KeyPress::returnKey));
        alert.runModalLoop();
        mSpeakerViewComponent->setShowTriplets(false);
        return;
    }*/

    mData.project.viewSettings.showSpeakerTriplets = newState;
    mSpeakerViewComponent->setShowTriplets(newState);
}

//==============================================================================
void MainContentComponent::handleShowSourceLevel()
{
    auto & var{ mData.project.viewSettings.showSourceActivity };
    var = !var;
    // TODO : where to set this?
}

//==============================================================================
void MainContentComponent::handleShowSpeakerLevel()
{
    auto & var{ mData.project.viewSettings.showSpeakerLevels };
    var = !var;
    // TODO : where to set this?
}

//==============================================================================
void MainContentComponent::handleShowSphere()
{
    auto & var{ mData.project.viewSettings.showSphereOrCube };
    var = !var;
    mSpeakerViewComponent->setShowSphere(var);
}

//==============================================================================
void MainContentComponent::handleResetInputPositions()
{
    for (auto * input : mSourceModels) {
        input->resetPosition();
    }
}

//==============================================================================
void MainContentComponent::handleResetMeterClipping()
{
    for (auto vuMeter : mSourceVuMeterComponents) {
        vuMeter.value->resetClipping();
    }
    for (auto vuMeter : mSpeakerVuMeters) {
        vuMeter.value->resetClipping();
    }
}

//==============================================================================
void MainContentComponent::handleColorizeInputs()
{
    float hue{};
    auto const inc{ 1.0f / static_cast<float>(mData.project.sources.size() + 1) };
    for (auto source : mData.project.sources) {
        auto const colour{ juce::Colour::fromHSV(hue, 1, 0.75, 1) };
        source.value->colour = colour;
        mSourceVuMeterComponents[source.key].setSourceColour(colour);
        hue += inc;
    }
}

//==============================================================================
// Command manager methods.
void MainContentComponent::getAllCommands(juce::Array<juce::CommandID> & commands)
{
    // this returns the set of all commands that this target can perform.
    const juce::CommandID ids[] = {
        MainWindow::NewProjectID,
        MainWindow::OpenProjectID,
        MainWindow::SaveProjectID,
        MainWindow::SaveAsProjectID,
        MainWindow::OpenSpeakerSetupID,
        MainWindow::ShowSpeakerEditID,
        MainWindow::Show2DViewID,
        MainWindow::ShowNumbersID,
        MainWindow::ShowSpeakersID,
        MainWindow::ShowTripletsID,
        MainWindow::ShowSourceLevelID,
        MainWindow::ShowSpeakerLevelID,
        MainWindow::ShowSphereID,
        MainWindow::ColorizeInputsID,
        MainWindow::ResetInputPosID,
        MainWindow::ResetMeterClipping,
        MainWindow::ShowOscLogView,
        MainWindow::OpenSettingsWindowID,
        MainWindow::QuitID,
        MainWindow::AboutID,
        MainWindow::OpenManualID,
    };

    commands.addArray(ids, juce::numElementsInArray(ids));
}

//==============================================================================
void MainContentComponent::getCommandInfo(juce::CommandID const commandId, juce::ApplicationCommandInfo & result)
{
    const juce::String generalCategory("General");

    switch (commandId) {
    case MainWindow::NewProjectID:
        result.setInfo("New Project", "Close the current project and open the default.", generalCategory, 0);
        result.addDefaultKeypress('N', juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::OpenProjectID:
        result.setInfo("Open Project", "Choose a new project on disk.", generalCategory, 0);
        result.addDefaultKeypress('O', juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::SaveProjectID:
        result.setInfo("Save Project", "Save the current project on disk.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::SaveAsProjectID:
        result.setInfo("Save Project As...", "Save the current project under a new name on disk.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::shiftModifier | juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::OpenSpeakerSetupID:
        result.setInfo("Load Speaker Setup", "Choose a new speaker setup on disk.", generalCategory, 0);
        result.addDefaultKeypress('L', juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::ShowSpeakerEditID:
        result.setInfo("Speaker Setup Edition", "Edit the current speaker setup.", generalCategory, 0);
        result.addDefaultKeypress('W', juce::ModifierKeys::altModifier);
        break;
    case MainWindow::Show2DViewID:
        result.setInfo("Show 2D View", "Show the 2D action window.", generalCategory, 0);
        result.addDefaultKeypress('D', juce::ModifierKeys::altModifier);
        break;
    case MainWindow::ShowNumbersID:
        result.setInfo("Show Numbers", "Show source and speaker numbers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('N', juce::ModifierKeys::altModifier);
        result.setTicked(mData.project.viewSettings.showSpeakerNumbers);
        break;
    case MainWindow::ShowSpeakersID:
        result.setInfo("Show Speakers", "Show speakers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::altModifier);
        result.setTicked(mData.project.viewSettings.showSpeakers);
        break;
    case MainWindow::ShowTripletsID:
        result.setInfo("Show Speaker Triplets", "Show speaker triplets on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('T', juce::ModifierKeys::altModifier);
        result.setTicked(mData.project.viewSettings.showSpeakerTriplets);
        break;
    case MainWindow::ShowSourceLevelID:
        result.setInfo("Show Source Activity", "Activate brightness on sources on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('A', juce::ModifierKeys::altModifier);
        result.setTicked(mData.project.viewSettings.showSourceActivity);
        break;
    case MainWindow::ShowSpeakerLevelID:
        result.setInfo("Show Speaker Level", "Activate brightness on speakers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('L', juce::ModifierKeys::altModifier);
        result.setTicked(mData.project.viewSettings.showSpeakerLevels);
        break;
    case MainWindow::ShowSphereID:
        result.setInfo("Show Sphere/Cube", "Show the sphere on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('O', juce::ModifierKeys::altModifier);
        result.setTicked(mData.project.viewSettings.showSphereOrCube);
        break;
    case MainWindow::ColorizeInputsID:
        result.setInfo("Colorize Inputs", "Spread the colour of the inputs over the colour range.", generalCategory, 0);
        result.addDefaultKeypress('C', juce::ModifierKeys::altModifier);
        break;
    case MainWindow::ResetInputPosID:
        result.setInfo("Reset Input Position", "Reset the position of the input sources.", generalCategory, 0);
        result.addDefaultKeypress('R', juce::ModifierKeys::altModifier);
        break;
    case MainWindow::ResetMeterClipping:
        result.setInfo("Reset Meter Clipping", "Reset clipping for all meters.", generalCategory, 0);
        result.addDefaultKeypress('M', juce::ModifierKeys::altModifier);
        break;
    case MainWindow::ShowOscLogView:
        result.setInfo("Show OSC Log Window", "Show the OSC logging window.", generalCategory, 0);
        break;
    case MainWindow::OpenSettingsWindowID:
        result.setInfo("Settings...", "Open the settings window.", generalCategory, 0);
        result.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::QuitID:
        result.setInfo("Quit", "Quit the SpatGRIS.", generalCategory, 0);
        result.addDefaultKeypress('Q', juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::AboutID:
        result.setInfo("About SpatGRIS", "Open the about window.", generalCategory, 0);
        break;
    case MainWindow::OpenManualID:
        result.setInfo("Open Documentation", "Open the manual in pdf viewer.", generalCategory, 0);
        break;
    default:
        break;
    }
}

//==============================================================================
bool MainContentComponent::perform(const InvocationInfo & info)
{
    if (MainWindow::getMainAppWindow()) {
        switch (info.commandID) {
        case MainWindow::NewProjectID:
            handleNew();
            break;
        case MainWindow::OpenProjectID:
            handleOpenProject();
            break;
        case MainWindow::SaveProjectID:
            handleSaveProject();
            break;
        case MainWindow::SaveAsProjectID:
            handleSaveAsProject();
            break;
        case MainWindow::OpenSpeakerSetupID:
            handleOpenSpeakerSetup();
            break;
        case MainWindow::ShowSpeakerEditID:
            handleShowSpeakerEditWindow();
            break;
        case MainWindow::Show2DViewID:
            handleShow2DView();
            break;
        case MainWindow::ShowNumbersID:
            handleShowNumbers();
            break;
        case MainWindow::ShowSpeakersID:
            handleShowSpeakers();
            break;
        case MainWindow::ShowTripletsID:
            handleShowTriplets();
            break;
        case MainWindow::ShowSourceLevelID:
            handleShowSourceLevel();
            break;
        case MainWindow::ShowSpeakerLevelID:
            handleShowSpeakerLevel();
            break;
        case MainWindow::ShowSphereID:
            handleShowSphere();
            break;
        case MainWindow::ColorizeInputsID:
            handleColorizeInputs();
            break;
        case MainWindow::ResetInputPosID:
            handleResetInputPositions();
            break;
        case MainWindow::ResetMeterClipping:
            handleResetMeterClipping();
            break;
        case MainWindow::ShowOscLogView:
            handleShowOscLogView();
            break;
        case MainWindow::OpenSettingsWindowID:
            handleShowPreferences();
            break;
        case MainWindow::QuitID:
            dynamic_cast<MainWindow *>(&mMainWindow)->closeButtonPressed();
            break;
        case MainWindow::AboutID:
            handleShowAbout();
            break;
        case MainWindow::OpenManualID:
            handleOpenManual();
            break;
        default:
            return false;
        }
    }
    return true;
}

//==============================================================================
void MainContentComponent::audioParametersChanged()
{
    juce::ScopedLock const lock{ mAudioProcessor->getCriticalSection() };

    auto * currentAudioDevice{ AudioManager::getInstance().getAudioDeviceManager().getCurrentAudioDevice() };
    jassert(currentAudioDevice);
    if (!currentAudioDevice) {
        return;
    }

    auto const deviceTypeName{ currentAudioDevice->getTypeName() };
    auto const setup{ AudioManager::getInstance().getAudioDeviceManager().getAudioDeviceSetup() };

    auto const sampleRate{ currentAudioDevice->getCurrentSampleRate() };
    auto const bufferSize{ currentAudioDevice->getCurrentBufferSizeSamples() };
    auto const inputCount{ currentAudioDevice->getActiveInputChannels().countNumberOfSetBits() };
    auto const outputCount{ currentAudioDevice->getActiveOutputChannels().countNumberOfSetBits() };

    mData.appData.audioSettings.sampleRate = setup.sampleRate;
    mData.appData.audioSettings.bufferSize = setup.bufferSize;
    mData.appData.audioSettings.deviceType = deviceTypeName;
    mData.appData.audioSettings.inputDevice = setup.inputDeviceName;
    mData.appData.audioSettings.outputDevice = setup.outputDeviceName;

    mSampleRateLabel->setText(juce::String{ narrow<unsigned>(sampleRate) } + " Hz",
                              juce::NotificationType::dontSendNotification);
    mBufferSizeLabel->setText(juce::String{ bufferSize } + " samples", juce::NotificationType::dontSendNotification);
    mChannelCountLabel->setText("I : " + juce::String{ inputCount } + " - O : " + juce::String{ outputCount },
                                juce::dontSendNotification);
}

//==============================================================================
juce::PopupMenu MainContentComponent::getMenuForIndex(int /*menuIndex*/, const juce::String & menuName)
{
    juce::ApplicationCommandManager * commandManager = &mMainWindow.getApplicationCommandManager();

    juce::PopupMenu menu;

    if (menuName == "File") {
        menu.addCommandItem(commandManager, MainWindow::NewProjectID);
        menu.addCommandItem(commandManager, MainWindow::OpenProjectID);
        menu.addCommandItem(commandManager, MainWindow::SaveProjectID);
        menu.addCommandItem(commandManager, MainWindow::SaveAsProjectID);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::OpenSpeakerSetupID);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::OpenSettingsWindowID);
#if !JUCE_MAC
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::QuitID);
#endif
    } else if (menuName == "View") {
        menu.addCommandItem(commandManager, MainWindow::Show2DViewID);
        menu.addCommandItem(commandManager, MainWindow::ShowSpeakerEditID);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::ShowNumbersID);
        menu.addCommandItem(commandManager, MainWindow::ShowSpeakersID);
        if (mSpatAlgorithm->hasTriplets()) {
            menu.addCommandItem(commandManager, MainWindow::ShowTripletsID);
        } else {
            menu.addItem(MainWindow::ShowTripletsID, "Show Speaker Triplets", false, false);
        }
        menu.addCommandItem(commandManager, MainWindow::ShowSourceLevelID);
        menu.addCommandItem(commandManager, MainWindow::ShowSpeakerLevelID);
        menu.addCommandItem(commandManager, MainWindow::ShowSphereID);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::ColorizeInputsID);
        menu.addCommandItem(commandManager, MainWindow::ResetInputPosID);
        menu.addCommandItem(commandManager, MainWindow::ResetMeterClipping);
    } else if (menuName == "Help") {
        menu.addCommandItem(commandManager, MainWindow::AboutID);
        menu.addCommandItem(commandManager, MainWindow::OpenManualID);
    }
    return menu;
}

//==============================================================================
void MainContentComponent::menuItemSelected(int /*menuItemID*/, int /*topLevelMenuIndex*/)
{
}

//==============================================================================
// Exit functions.
bool MainContentComponent::isProjectModified() const
{
    auto const savedState{ juce::XmlDocument{ mData.appData.lastProject }.getDocumentElement() };
    if (!savedState) {
        return true;
    }

    std::unique_ptr<juce::XmlElement> const currentState{ mData.appData.toXml() };
    jassert(currentState);

    return !savedState->isEquivalentTo(currentState.get(), true);
}

//==============================================================================
bool MainContentComponent::exitApp()
{
    // TODO : maybe the audio settings should be part of the project?

    auto exitV{ 2 };
    juce::XmlDocument lastProjectDocument{ mData.appData.lastProject };
    auto lastProjectElement{ lastProjectDocument.getDocumentElement() };
    std::unique_ptr<juce::XmlElement> currentProjectElement{ mData.appData.toXml() };

    if (isProjectModified()) {
        juce::AlertWindow alert("Exit SpatGRIS !",
                                "Do you want to save the current project ?",
                                juce::AlertWindow::InfoIcon);
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        alert.addButton("Exit", 2, juce::KeyPress(juce::KeyPress::deleteKey));
        exitV = alert.runModalLoop();
        if (exitV == 1) {
            alert.setVisible(false);
            juce::ModalComponentManager::getInstance()->cancelAllModalComponents();

            juce::FileChooser fc("Choose a file to save...", mData.appData.lastProject, "*.xml", true);

            if (fc.browseForFileToSave(true)) {
                auto const chosen{ fc.getResults().getReference(0).getFullPathName() };
                saveProject(chosen);
            } else {
                exitV = 0;
            }
        }
    }

    return exitV != 0;
}

//==============================================================================
void MainContentComponent::refreshVuMeterPeaks()
{
    auto & audioData{ mAudioProcessor->getAudioData() };
    auto const & sourcePeaks{ *audioData.sourcePeaks.get() };
    for (auto const peak : sourcePeaks) {
        dbfs_t const dbPeak{ peak.value };
        mSourceVuMeterComponents[peak.key].setLevel(dbPeak);
    }

    auto const & speakerPeaks{ *audioData.speakerPeaks.get() };
    for (auto const peak : speakerPeaks) {
        dbfs_t dbPeak{ peak.value };
        mSpeakerVuMeters[peak.key].setLevel(dbPeak);
    }
}

//==============================================================================
void MainContentComponent::refreshSourceVuMeterComponents()
{
    mSourceVuMeterComponents.clear();

    auto x{ 2 };
    for (auto source : mData.project.sources) {
        auto newVuMeter{ std::make_unique<SourceVuMeterComponent>(source.key,
                                                                  source.value->directOut,
                                                                  source.value->colour,
                                                                  *this,
                                                                  mSmallLookAndFeel) };
        mInputsUiBox->addAndMakeVisible(newVuMeter.get());
        juce::Rectangle<int> const bounds{ x, 4, VU_METER_WIDTH_IN_PIXELS, 200 };
        newVuMeter->setBounds(bounds);
        mSourceVuMeterComponents.add(source.key, std::move(newVuMeter));
        x += VU_METER_WIDTH_IN_PIXELS;
    }
}

//==============================================================================
void MainContentComponent::refreshSpeakerVuMeterComponents()
{
    mSpeakerVuMeters.clear();

    auto x{ 2 };
    for (auto speaker : mData.speakerSetup.speakers) {
        auto newVuMeter{ std::make_unique<SpeakerVuMeterComponent>(speaker.key, *this, mSmallLookAndFeel) };
        mOutputsUiBox->addAndMakeVisible(newVuMeter.get());
        juce::Rectangle<int> const bounds{ x, 4, VU_METER_WIDTH_IN_PIXELS, 200 };
        newVuMeter->setBounds(bounds);
        mSpeakerVuMeters.add(speaker.key, std::move(newVuMeter));
        x += VU_METER_WIDTH_IN_PIXELS;
    }
}

//==============================================================================
void MainContentComponent::handleSourceColorChanged(source_index_t const sourceIndex, juce::Colour const colour)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mData.project.sources[sourceIndex].colour = colour;
    mSourceVuMeterComponents[sourceIndex].setSourceColour(colour);
}

//==============================================================================
void MainContentComponent::handleSourceStateChanged(source_index_t const sourceIndex, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mData.project.sources[sourceIndex].state = state;

    mAudioProcessor->setAudioConfig(mData.toAudioConfig());
    mSourceVuMeterComponents[sourceIndex].setState(state);
}

//==============================================================================
void MainContentComponent::handleSpeakerSelected(juce::Array<output_patch_t> const selection)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    for (auto const speaker : mData.speakerSetup.speakers) {
        auto const isSelected{ selection.contains(speaker.key) };

        if (speaker.value->isSelected == isSelected) {
            continue;
        }

        speaker.value->isSelected = isSelected;
        mSpeakerVuMeters[speaker.key].setSelected(isSelected);
        if (mEditSpeakersWindow) {
            // TODO : handle multiple selection
            mEditSpeakersWindow->selectSpeaker(speaker.key);
        }
        // TODO : update 3D view ?
    }
}

//==============================================================================
void MainContentComponent::handleSpeakerStateChanged(output_patch_t const outputPatch, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mData.speakerSetup.speakers[outputPatch].state = state;

    mAudioProcessor->setAudioConfig(mData.toAudioConfig());
    mSpeakerVuMeters[outputPatch].setState(state);
    // TODO : update 3D view ?
}

//==============================================================================
void MainContentComponent::handleSourceDirectOutChanged(source_index_t const sourceIndex,
                                                        tl::optional<output_patch_t> const outputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mData.project.sources[sourceIndex].directOut = outputPatch;

    mAudioProcessor->setAudioConfig(mData.toAudioConfig());
    mSourceVuMeterComponents[sourceIndex].setDirectOut(outputPatch);
}

//==============================================================================
void MainContentComponent::handleSpatModeChanged(SpatMode const spatMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (mData.appData.spatMode != spatMode) {
        mData.appData.spatMode = spatMode;

        switch (spatMode) {
        case SpatMode::hrtfVbap:
            loadSpeakerSetup(BINAURAL_SPEAKER_SETUP_FILE, SpatMode::hrtfVbap);
            mAudioProcessor->resetHrtf();
            break;
        case SpatMode::stereo:
            loadSpeakerSetup(STEREO_SPEAKER_SETUP_FILE, SpatMode::stereo);
        case SpatMode::lbap:
        case SpatMode::vbap:
            break;
        }

        mSpatAlgorithm = AbstractSpatAlgorithm::make(spatMode);
        mSpatAlgorithm->init(mData.speakerSetup.speakers);
        mSpatModeCombo->setSelectedId(static_cast<int>(spatMode), juce::dontSendNotification);
    }
}

//==============================================================================
void MainContentComponent::handleMasterGainChanged(dbfs_t const gain)
{
    mData.project.masterGain = gain;
    mAudioProcessor->setAudioConfig(mData.toAudioConfig());
    mMasterGainOutSlider->setValue(gain.get(), juce::dontSendNotification);
}

//==============================================================================
void MainContentComponent::handleGainInterpolationChanged(float const interpolation)
{
    mData.project.spatGainsInterpolation = interpolation;
    mAudioProcessor->setAudioConfig(mData.toAudioConfig());
    mInterpolationSlider->setValue(interpolation, juce::dontSendNotification);
}

//==============================================================================
void MainContentComponent::setSourceState(source_index_t const sourceIndex, PortState const state)
{
    mData.project.sources[sourceIndex].state = state;
}

//==============================================================================
void MainContentComponent::setSpeakerState(output_patch_t const outputPatch, PortState const state)
{
    mData.speakerSetup.speakers[outputPatch].state = state;
}

//==============================================================================
bool MainContentComponent::tripletExists(Triplet const & tri, int & pos) const
{
    pos = 0;
    for (auto const & ti : mTriplets) {
        if ((ti.id1 == tri.id1 && ti.id2 == tri.id2 && ti.id3 == tri.id3)
            || (ti.id1 == tri.id1 && ti.id2 == tri.id3 && ti.id3 == tri.id2)
            || (ti.id1 == tri.id2 && ti.id2 == tri.id1 && ti.id3 == tri.id3)
            || (ti.id1 == tri.id2 && ti.id2 == tri.id3 && ti.id3 == tri.id1)
            || (ti.id1 == tri.id3 && ti.id2 == tri.id2 && ti.id3 == tri.id1)
            || (ti.id1 == tri.id3 && ti.id2 == tri.id1 && ti.id3 == tri.id2)) {
            return true;
        }
        pos += 1;
    }

    return false;
}

//==============================================================================
void MainContentComponent::reorderSpeakers(juce::Array<output_patch_t> newOrder)
{
    juce::ScopedLock lock{ mCriticalSection }; // TODO: necessary?
    auto & order{ mData.speakerSetup.order };
    jassert(newOrder.size() == order.size());
    order = std::move(newOrder);
    // TODO : reorder components
}

//==============================================================================
output_patch_t MainContentComponent::getMaxSpeakerOutputPatch() const
{
    auto const & speakers{ mData.speakerSetup.speakers };
    auto const maxNode{ std::max_element(
        speakers.cbegin(),
        speakers.cend(),
        [](SpeakersData::Node const & a, SpeakersData::Node const & b) { return a.key < b.key; }) };
    return (*maxNode).key;
}

//==============================================================================
output_patch_t MainContentComponent::addSpeaker()
{
    juce::ScopedLock const lock{ mCriticalSection };
    auto const newOutputPatch{ ++getMaxSpeakerOutputPatch() };
    mData.speakerSetup.speakers.add(newOutputPatch, std::make_unique<SpeakerData>());
    mData.speakerSetup.order.add(newOutputPatch);
    return newOutputPatch;
}

//==============================================================================
void MainContentComponent::insertSpeaker(int const position)
{
    auto const newPosition{ position + 1 };

    juce::ScopedLock const lock{ mCriticalSection };
    auto const newOutputPatch{ addSpeaker() };
    auto & order{ mData.speakerSetup.order };
    [[maybe_unused]] auto const lastAppenedOutputPatch{ mData.speakerSetup.order.getLast() };
    jassert(newOutputPatch == lastAppenedOutputPatch);
    order.removeLast();
    order.insert(newPosition, newOutputPatch);
}

//==============================================================================
void MainContentComponent::removeSpeaker(output_patch_t const outputPatch)
{
    juce::ScopedLock const lock{ mCriticalSection };
    mSpeakerModels.remove(outputPatch);
    mSpeakerVuMeters.remove(outputPatch);
    mData.speakerSetup.order.removeFirstMatchingValue(outputPatch);
    mData.speakerSetup.speakers.remove(outputPatch);
}

//==============================================================================
bool MainContentComponent::isRadiusNormalized() const
{
    return mData.appData.spatMode == SpatMode::vbap || mData.appData.spatMode == SpatMode::hrtfVbap;
}

//==============================================================================
void MainContentComponent::updateSourceData(int const sourceDataIndex, InputModel & input) const
{
    jassertfalse;
    /*if (sourceDataIndex >= mAudioProcessor->getSourcesIn().size()) {
        return;
    }

    auto const spatMode{ mAudioProcessor->getMode() };
    auto & sourceData{ mAudioProcessor->getSourcesIn()[sourceDataIndex] };

    if (spatMode == SpatMode::lbap) {
        sourceData.radAzimuth = input.getAzimuth();
        sourceData.radElevation = HALF_PI - input.getZenith();
    } else {
        sourceData.azimuth = input.getAzimuth().toDegrees();
        if (sourceData.azimuth > degrees_t{ 180.0f }) {
            sourceData.azimuth = sourceData.azimuth - degrees_t{ 360.0f };
        }
        sourceData.zenith = degrees_t{ 90.0f } - input.getZenith().toDegrees();
    }
    sourceData.radius = input.getRadius();

    sourceData.azimuthSpan = input.getAzimuthSpan() * 0.5f;
    sourceData.zenithSpan = input.getZenithSpan() * 2.0f;

    if (spatMode == SpatMode::vbap || spatMode == SpatMode::hrtfVbap) {
        sourceData.shouldUpdateVbap = true;
    }*/
}

//==============================================================================
void MainContentComponent::setTripletsFromVbap()
{
    jassert(mSpatAlgorithm->hasTriplets());
    mTriplets = mSpatAlgorithm->getTriplets();
}

//==============================================================================
void MainContentComponent::handleNumSourcesChanged(int const numSources)
{
    jassert(numSources >= 1 && numSources <= MAX_INPUTS);

    auto const removeSource = [&](source_index_t const index) {
        auto ** findResult{ std::find_if(mSourceModels.begin(),
                                         mSourceModels.end(),
                                         [index](InputModel const * source) { return source->getIndex() == index; }) };
        jassert(findResult != mSourceModels.end());
        mSourceModels.removeObject(*findResult);

        mSourceVuMeterComponents.remove(index);
        mData.project.sources.remove(index);
    };

    auto const addSource = [&](source_index_t const index) {
        mData.project.sources.add(index, std::make_unique<SourceData>());
        mSourceModels.add(std::make_unique<InputModel>(*this, mSmallLookAndFeel, index));
        mSourceVuMeterComponents.add(
            index,
            std::make_unique<SourceVuMeterComponent>(index, tl::nullopt, juce::Colour{}, *this, mSmallLookAndFeel));
    };

    mNumSourcesTextEditor->setText(juce::String{ numSources }, false);

    if (numSources > mSourceModels.size()) {
        source_index_t const firstNewIndex{ mData.project.sources.size() + 1 };
        source_index_t const lastNewIndex{ numSources };
        for (auto index{ firstNewIndex }; index <= lastNewIndex; ++index) {
            addSource(index);
        }
    } else if (numSources < mSourceModels.size()) {
        // remove some inputs
        while (mData.project.sources.size() > numSources) {
            source_index_t const index{ mData.project.sources.size() };
            removeSource(index);
        }
    }
    unfocusAllComponents();
    refreshSpeakers();
}

//==============================================================================
static SpeakerHighpassConfig linkwitzRileyComputeVariables(double const freq, double const sr)
{
    auto const wc{ 2.0 * juce::MathConstants<double>::pi * freq };
    auto const wc2{ wc * wc };
    auto const wc3{ wc2 * wc };
    auto const wc4{ wc2 * wc2 };
    auto const k{ wc / std::tan(juce::MathConstants<double>::pi * freq / sr) };
    auto const k2{ k * k };
    auto const k3{ k2 * k };
    auto const k4{ k2 * k2 };
    static auto constexpr SQRT2{ juce::MathConstants<double>::sqrt2 };
    auto const sqTmp1{ SQRT2 * wc3 * k };
    auto const sqTmp2{ SQRT2 * wc * k3 };
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
dbfs_t MainContentComponent::getSourcePeak(source_index_t const sourceIndex) const
{
    auto const & peaks{ *mAudioProcessor->getAudioData().sourcePeaks.get() };
    auto const magnitude{ peaks[sourceIndex] };
    return dbfs_t::fromGain(magnitude);
}

//==============================================================================
float MainContentComponent::getSourceAlpha(source_index_t const sourceIndex) const
{
    auto const db{ getSourcePeak(sourceIndex) };
    auto const level{ db.toGain() };
    if (level > 0.0001f) {
        // -80 dB
        return 1.0f;
    }
    return std::sqrt(level * 10000.0f);
}

//==============================================================================
dbfs_t MainContentComponent::getSpeakerPeak(output_patch_t const outputPatch) const
{
    auto const & peaks{ *mAudioProcessor->getAudioData().speakerPeaks.get() };
    auto const magnitude{ peaks[outputPatch] };
    return dbfs_t::fromGain(magnitude);
}

//==============================================================================
float MainContentComponent::getSpeakerAlpha(output_patch_t const outputPatch) const
{
    auto const db{ getSpeakerPeak(outputPatch) };
    auto const level{ db.toGain() };
    float alpha;
    if (level > 0.001f) {
        // -60 dB
        alpha = 1.0f;
    } else {
        alpha = std::sqrt(level * 1000.0f);
    }
    if (alpha < 0.6f) {
        alpha = 0.6f;
    }
    return alpha;
}

//==============================================================================
bool MainContentComponent::refreshSpeakers()
{
    // TODO : this function is 100 times longer than it should be.

    auto const & speakers{ mData.speakerSetup.speakers };
    auto const numActiveSpeakers{ std::count_if(
        speakers.cbegin(),
        speakers.cend(),
        [](SpeakersData::Node const speaker) { return speaker.value->isDirectOutOnly; }) };

    // Ensure there is enough speakers
    auto const showNotEnoughSpeakersError = [&]() {
        juce::AlertWindow alert("Not enough speakers !    ",
                                "Do you want to reload the default setup ?    ",
                                juce::AlertWindow::WarningIcon);
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("No", 0);
        alert.addButton("Yes", 1, juce::KeyPress(juce::KeyPress::returnKey));
        if (alert.runModalLoop() != 0) {
            loadSpeakerSetup(DEFAULT_SPEAKER_SETUP_FILE);
        }
    };

    if (numActiveSpeakers < 2) {
        showNotEnoughSpeakersError();
        return false;
    }

    auto const getVbapDimensions = [&]() {
        auto const firstSpeaker{ *mData.speakerSetup.speakers.begin() };
        auto const firstZenith{ firstSpeaker.value->vector.elevation };
        auto const minZenith{ firstZenith - degrees_t{ 4.9f } };
        auto const maxZenith{ firstZenith + degrees_t{ 4.9f } };

        auto const areSpeakersOnSamePlane{ std::all_of(mData.speakerSetup.speakers.cbegin(),
                                                       mData.speakerSetup.speakers.cend(),
                                                       [&](SpeakersData::Node const node) {
                                                           auto const zenith{ node.value->vector.elevation };
                                                           return zenith < maxZenith && zenith > minZenith;
                                                       }) };
        return areSpeakersOnSamePlane ? VbapType::twoD : VbapType::threeD;
    };

    auto const lbapDimensions{ getVbapDimensions() };
    if (lbapDimensions == VbapType::twoD) {
        mData.project.viewSettings.showSpeakerTriplets = false;
    } else if (mData.speakerSetup.speakers.size() < 3) {
        showNotEnoughSpeakersError();
        return false;
    }

    // Test for duplicated output patch.
    auto const testDuplicatedOutputPatch = [&]() {
        jassert(mData.speakerSetup.order.size() == speakers.size());
        auto outputPatches{ mData.speakerSetup.order };
        std::sort(outputPatches.begin(), outputPatches.end());
        auto const duplicate{ std::adjacent_find(outputPatches.begin(), outputPatches.end()) };
        return duplicate != outputPatches.end();
    };

    if (testDuplicatedOutputPatch()) {
        juce::AlertWindow alert{ "Duplicated Output Numbers!    ",
                                 "Some output numbers are used more than once. Do you want to continue anyway?    "
                                 "\nIf you continue, you may have to fix your speaker setup before using it!   ",
                                 juce::AlertWindow::WarningIcon };
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("Load default setup", 0);
        alert.addButton("Keep current setup", 1);
        if (alert.runModalLoop() == 0) {
            loadSpeakerSetup(DEFAULT_SPEAKER_SETUP_FILE);
            mNeedToSaveSpeakerSetup = false;
        }
        return false;
    }

    refreshSourceVuMeterComponents();
    refreshSpeakerVuMeterComponents();

    mSpatAlgorithm->init(mData.speakerSetup.speakers);

    if (mEditSpeakersWindow != nullptr) {
        mEditSpeakersWindow->updateWinContent(false);
    }

    mOutputsUiBox->repaint();
    resized();

    /*} else {
        juce::AlertWindow alert{ "Not a valid DOME 3-D configuration!    ",
                                 "Maybe you want to open it in CUBE mode? Reload the default speaker setup ?    ",
                                 juce::AlertWindow::WarningIcon };
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("Ok", 0, juce::KeyPress(juce::KeyPress::returnKey));
        alert.runModalLoop();
        openXmlFileSpeaker(DEFAULT_SPEAKER_SETUP_FILE);
        return false;
    }*/

    mAudioProcessor->setAudioConfig(mData.toAudioConfig());

    return true;
}

//==============================================================================
void MainContentComponent::setCurrentSpeakerSetup(juce::File const & file)
{
    mCurrentSpeakerSetup = file;
    mConfigurationName = file.getFileNameWithoutExtension();
    mSpeakerViewComponent->setNameConfig(mConfigurationName);
}

//==============================================================================
void MainContentComponent::reloadXmlFileSpeaker()
{
    loadSpeakerSetup(mData.appData.lastSpeakerSetup, tl::nullopt);
}

//==============================================================================
void MainContentComponent::loadSpeakerSetup(juce::File const & file, tl::optional<SpatMode> const forceSpatMode)
{
    jassert(file.existsAsFile());

    if (!file.existsAsFile()) {
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon,
                                          "Error in Load Speaker Setup !",
                                          "Cannot find file " + file.getFullPathName() + ", loading default setup.");
        loadSpeakerSetup(DEFAULT_SPEAKER_SETUP_FILE);
        return;
    }

    juce::XmlDocument xmlDoc{ file };
    auto const mainXmlElem(xmlDoc.getDocumentElement());
    if (!mainXmlElem) {
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon,
                                          "Error in Load Speaker Setup !",
                                          "Your file is corrupted !\n" + xmlDoc.getLastParseError());
        loadSpeakerSetup(DEFAULT_SPEAKER_SETUP_FILE);
        return;
    }

    auto speakerSetup{ SpeakerSetup::fromXml(*mainXmlElem) };

    if (!speakerSetup) {
        auto const msg{ mainXmlElem->hasTagName("ServerGRIS_Preset")
                            ? "You are trying to open a Server document, and not a Speaker Setup !"
                            : "Your file is corrupted !\n" + xmlDoc.getLastParseError() };
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Error in Load Speaker Setup !", msg);
        loadSpeakerSetup(DEFAULT_SPEAKER_SETUP_FILE);
        return;
    }

    juce::ScopedLock const lock{ mCriticalSection };

    mData.speakerSetup = std::move(speakerSetup->first);

    if (forceSpatMode) {
        handleSpatModeChanged(*forceSpatMode);
    } else if (mData.appData.spatMode != speakerSetup->second) {
        // TODO : show dialog and choose spatMode
        handleSpatModeChanged(speakerSetup->second);
    }

    refreshSpeakers();
}

//==============================================================================
void MainContentComponent::setTitle() const
{
    auto const currentProject{ mData.appData.lastProject };
    auto const title{ juce::String{ "SpatGRIS v" } + juce::JUCEApplication::getInstance()->getApplicationVersion()
                      + " - " + currentProject };
    mMainWindow.setName(title);
}

//==============================================================================
void MainContentComponent::handleTimer(bool const state)
{
    if (state) {
        startTimerHz(24);
    } else {
        stopTimer();
    }
}

//==============================================================================
void MainContentComponent::saveProject(juce::String const & path)
{
    juce::File const xmlFile{ path };
    auto const xml{ mData.project.toXml() };
    [[maybe_unused]] auto success{ xml->writeTo(xmlFile) };
    jassert(success);
    success = xmlFile.create();
    jassert(success);
    mData.appData.lastProject = path;

    setTitle();
}

//==============================================================================
void MainContentComponent::saveSpeakerSetup(juce::String const & path)
{
    juce::File const xmlFile{ path };
    auto const xml{ mData.speakerSetup.toXml(mData.appData.spatMode) };

    [[maybe_unused]] auto success{ xml->writeTo(xmlFile) };
    jassert(success);
    success = xmlFile.create();
    jassert(success);

    mData.appData.lastSpeakerSetup = path;

    setCurrentSpeakerSetup(path);
}

//==============================================================================
void MainContentComponent::timerCallback()
{
    // Update levels
    refreshVuMeterPeaks();

    auto & audioManager{ AudioManager::getInstance() };
    auto & audioDeviceManager{ audioManager.getAudioDeviceManager() };
    auto * audioDevice{ audioDeviceManager.getCurrentAudioDevice() };

    if (!audioDevice) {
        return;
    }

    // TODO : static variables no good
    static double cpuRunningAverage{};
    static double amountToRemove{};
    auto const currentCpuUsage{ audioDeviceManager.getCpuUsage() * 100.0 };
    if (currentCpuUsage > cpuRunningAverage) {
        cpuRunningAverage = currentCpuUsage;
        amountToRemove = 0.01;
    } else {
        cpuRunningAverage = std::max(cpuRunningAverage - amountToRemove, currentCpuUsage);
        amountToRemove *= 1.1;
    }

    auto const cpuLoad{ narrow<int>(std::round(cpuRunningAverage)) };
    mCpuUsageValue->setText(juce::String{ cpuLoad } + " %", juce::dontSendNotification);

    auto const sampleRate{ audioDevice->getCurrentSampleRate() };
    auto seconds{ static_cast<int>(static_cast<double>(audioManager.getNumSamplesRecorded()) / sampleRate) };
    auto const minute{ seconds / 60 % 60 };
    seconds = seconds % 60;
    auto const timeRecorded{ ((minute < 10) ? "0" + juce::String{ minute } : juce::String{ minute }) + " : "
                             + ((seconds < 10) ? "0" + juce::String{ seconds } : juce::String{ seconds }) };
    mTimeRecordedLabel->setText(timeRecorded, juce::dontSendNotification);

    if (mStartRecordButton->getToggleState()) {
        mStartRecordButton->setToggleState(false, juce::dontSendNotification);
    }

    if (audioManager.isRecording()) {
        mStartRecordButton->setButtonText("Stop");
    } else {
        mStartRecordButton->setButtonText("Record");
    }

    if (cpuLoad >= 100) {
        mCpuUsageValue->setColour(juce::Label::backgroundColourId, juce::Colours::darkred);
    } else {
        mCpuUsageValue->setColour(juce::Label::backgroundColourId, mLookAndFeel.getWinBackgroundColour());
    }

    if (mIsProcessForeground != juce::Process::isForegroundProcess()) {
        mIsProcessForeground = juce::Process::isForegroundProcess();
        if (mEditSpeakersWindow != nullptr && mIsProcessForeground) {
            mEditSpeakersWindow->setVisible(true);
            mEditSpeakersWindow->setAlwaysOnTop(true);
        } else if (mEditSpeakersWindow != nullptr && !mIsProcessForeground) {
            mEditSpeakersWindow->setVisible(false);
            mEditSpeakersWindow->setAlwaysOnTop(false);
        }
        if (mFlatViewWindow != nullptr && mIsProcessForeground) {
            mFlatViewWindow->toFront(false);
            toFront(true);
        }
    }
}

//==============================================================================
void MainContentComponent::paint(juce::Graphics & g)
{
    g.fillAll(mLookAndFeel.getWinBackgroundColour());
}

//==============================================================================
void MainContentComponent::textEditorFocusLost(juce::TextEditor & textEditor)
{
    textEditorReturnKeyPressed(textEditor);
}

//==============================================================================
void MainContentComponent::textEditorReturnKeyPressed(juce::TextEditor & textEditor)
{
    jassert(&textEditor == mNumSourcesTextEditor.get());
    auto const unclippedValue{ mNumSourcesTextEditor->getTextValue().toString().getIntValue() };
    auto const numOfInputs{ std::clamp(unclippedValue, 2, MAX_INPUTS) };
    handleNumSourcesChanged(numOfInputs);
}

//==============================================================================
void MainContentComponent::buttonClicked(juce::Button * button)
{
    auto & audioManager{ AudioManager::getInstance() };

    if (button == mStartRecordButton.get()) {
        if (audioManager.isRecording()) {
            audioManager.stopRecording();
            mStartRecordButton->setEnabled(false);
            mTimeRecordedLabel->setColour(juce::Label::textColourId, mLookAndFeel.getFontColour());
        } else {
            audioManager.startRecording();
            mTimeRecordedLabel->setColour(juce::Label::textColourId, mLookAndFeel.getRedColour());
        }
        mStartRecordButton->setToggleState(audioManager.isRecording(), juce::dontSendNotification);
    } else if (button == mInitRecordButton.get()) {
        if (initRecording()) {
            mStartRecordButton->setEnabled(true);
        }
    }
}

//==============================================================================
void MainContentComponent::sliderValueChanged(juce::Slider * slider)
{
    if (slider == mMasterGainOutSlider.get()) {
        dbfs_t const value{ mMasterGainOutSlider->getValue() };
        handleMasterGainChanged(value);
    } else if (slider == mInterpolationSlider.get()) {
        auto const value{ static_cast<float>(mInterpolationSlider->getValue()) };
        handleGainInterpolationChanged(value);
    }
}

//==============================================================================
void MainContentComponent::comboBoxChanged(juce::ComboBox * comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == mSpatModeCombo.get()) {
        if (mNeedToSaveSpeakerSetup) {
            juce::AlertWindow alert(
                "The speaker configuration has changed!    ",
                "Save your changes or close the speaker configuration window before switching mode...    ",
                juce::AlertWindow::WarningIcon);
            alert.setLookAndFeel(&mLookAndFeel);
            alert.addButton("Ok", 0, juce::KeyPress(juce::KeyPress::returnKey));
            alert.runModalLoop();
            mSpatModeCombo->setSelectedId(static_cast<int>(mData.appData.spatMode) + 1,
                                          juce::NotificationType::dontSendNotification);
            return;
        }

        juce::ScopedLock const lock{ mAudioProcessor->getCriticalSection() };
        auto const newSpatMode{ static_cast<SpatMode>(mSpatModeCombo->getSelectedId() - 1) };
        handleSpatModeChanged(newSpatMode);

        if (mEditSpeakersWindow != nullptr) {
            auto const windowName{ juce::String("Speakers Setup Edition - ")
                                   + juce::String(MODE_SPAT_STRING[static_cast<int>(newSpatMode)]) + juce::String(" - ")
                                   + mCurrentSpeakerSetup.getFileName() };
            mEditSpeakersWindow->setName(windowName);
        }
    }
}

//==============================================================================
juce::StringArray MainContentComponent::getMenuBarNames()
{
    char const * names[] = { "File", "View", "Help", nullptr };
    return juce::StringArray{ names };
}

//==============================================================================
void MainContentComponent::setOscLogging(juce::OSCMessage const & message) const
{
    if (mOscLogWindow) {
        auto const address{ message.getAddressPattern().toString() };
        mOscLogWindow->addToLog(address + "\n");
        juce::String msg;
        for (auto const & element : message) {
            if (element.isInt32()) {
                msg += juce::String{ element.getInt32() } + " ";
            } else if (element.isFloat32()) {
                msg += juce::String{ element.getFloat32() } + " ";
            } else if (element.isString()) {
                msg += element.getString() + " ";
            }
        }

        mOscLogWindow->addToLog(msg + "\n");
    }
}

//==============================================================================
bool MainContentComponent::initRecording()
{
    juce::File const dir{ mData.appData.lastRecordingDirectory };
    juce::String extF;
    juce::String extChoice;

    auto const recordingFormat{ mData.appData.recordingOptions.format };

    if (recordingFormat == RecordingFormat::wav) {
        extF = ".wav";
        extChoice = "*.wav,*.aif";
    } else {
        extF = ".aif";
        extChoice = "*.aif,*.wav";
    }

    auto const recordingConfig{ mData.appData.recordingOptions.fileType };

    juce::FileChooser fc{ "Choose a file to save...", dir.getFullPathName() + "/recording" + extF, extChoice, true };

    if (!fc.browseForFileToSave(true)) {
        return false;
    }

    auto const filePath{ fc.getResults().getReference(0) };
    mData.appData.lastRecordingDirectory = juce::File{ filePath }.getParentDirectory().getFullPathName();
    RecordingOptions const recordingOptions{ filePath,
                                             recordingFormat,
                                             recordingConfig,
                                             narrow<double>(mSamplingRate) };
    return AudioManager::getInstance().prepareToRecord(recordingOptions, mSpeakerModels);
}

//==============================================================================
void MainContentComponent::resized()
{
    static constexpr auto MENU_BAR_HEIGHT = 20;
    static constexpr auto PADDING = 10;

    auto reducedLocalBounds{ getLocalBounds().reduced(2) };

    mMenuBar->setBounds(0, 0, getWidth(), MENU_BAR_HEIGHT);
    reducedLocalBounds.removeFromTop(MENU_BAR_HEIGHT);

    // Lay out the speaker view and the vertical divider.
    Component * vComps[] = { mSpeakerViewComponent.get(), mVerticalDividerBar.get(), nullptr };

    // Lay out side-by-side and resize the components' heights as well as widths.
    mVerticalLayout.layOutComponents(vComps,
                                     3,
                                     reducedLocalBounds.getX(),
                                     reducedLocalBounds.getY(),
                                     reducedLocalBounds.getWidth(),
                                     reducedLocalBounds.getHeight(),
                                     false,
                                     true);

    juce::Rectangle<int> const newMainUiBoxBounds{ mSpeakerViewComponent->getWidth() + 6,
                                                   MENU_BAR_HEIGHT,
                                                   getWidth() - (mSpeakerViewComponent->getWidth() + PADDING),
                                                   getHeight() };
    mMainUiBox->setBounds(newMainUiBoxBounds);
    mMainUiBox->correctSize(getWidth() - mSpeakerViewComponent->getWidth() - 6, 610);

    juce::Rectangle<int> const newInputsUiBoxBounds{ 0,
                                                     2,
                                                     getWidth() - (mSpeakerViewComponent->getWidth() + PADDING),
                                                     231 };
    mInputsUiBox->setBounds(newInputsUiBoxBounds);
    mInputsUiBox->correctSize(mSourceModels.size() * VU_METER_WIDTH_IN_PIXELS + 4, 200);

    juce::Rectangle<int> const newOutputsUiBoxBounds{ 0,
                                                      233,
                                                      getWidth() - (mSpeakerViewComponent->getWidth() + PADDING),
                                                      210 };
    mOutputsUiBox->setBounds(newOutputsUiBoxBounds);
    mOutputsUiBox->correctSize(mSpeakerModels.size() * VU_METER_WIDTH_IN_PIXELS + 4, 180);

    juce::Rectangle<int> const newControlUiBoxBounds{ 0,
                                                      443,
                                                      getWidth() - (mSpeakerViewComponent->getWidth() + PADDING),
                                                      145 };
    mControlUiBox->setBounds(newControlUiBoxBounds);
    mControlUiBox->correctSize(410, 145);
}
