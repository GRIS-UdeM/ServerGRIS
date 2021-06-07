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

#include "MainComponent.hpp"

#include "AudioManager.hpp"
#include "Constants.hpp"
#include "ControlPanel.hpp"
#include "FatalError.hpp"
#include "LegacyLbapPosition.hpp"
#include "MainWindow.hpp"
#include "TitledComponent.hpp"
#include "VuMeterComponent.hpp"

static constexpr auto BUTTON_CANCEL = 0;
static constexpr auto BUTTON_OK = 1;
static constexpr auto BUTTON_DISCARD = 2;

//==============================================================================
static float gainToSpeakerAlpha(float const gain)
{
    static constexpr auto MIN_ALPHA{ 0.1f };
    static constexpr auto MAX_ALPHA{ 1.0f };
    static constexpr auto ALPHA_RANGE{ MAX_ALPHA - MIN_ALPHA };
    static constexpr auto ALPHA_CURVE{ 0.7f };

    static constexpr dbfs_t MIN_DB{ -60.0f };
    static constexpr dbfs_t MAX_DB{ 0.0f };
    static constexpr dbfs_t DB_RANGE{ MAX_DB - MIN_DB };

    auto const clippedGain{ std::clamp(dbfs_t::fromGain(gain), MIN_DB, MAX_DB) };
    auto const ratio{ (clippedGain - MIN_DB) / DB_RANGE };
    jassert(ratio >= 0.0f && ratio <= 1.0f);
    auto const result{ std::pow(ratio, ALPHA_CURVE) * ALPHA_RANGE + MIN_ALPHA };

    return result;
}

//==============================================================================
static float gainToSourceAlpha(float const gain)
{
    static constexpr auto OFF = 0.02f;
    static constexpr auto ON = 0.8f;

    static constexpr dbfs_t MIN_GAIN{ -80.0f };

    return dbfs_t::fromGain(gain) > MIN_GAIN ? ON : OFF;
}

//==============================================================================
MainContentComponent::MainContentComponent(MainWindow & mainWindow,
                                           GrisLookAndFeel & grisLookAndFeel,
                                           SmallGrisLookAndFeel & smallGrisLookAndFeel)
    : mLookAndFeel(grisLookAndFeel)
    , mSmallLookAndFeel(smallGrisLookAndFeel)
    , mMainWindow(mainWindow)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto const showSplashScreen = [&]() {
#if NDEBUG
        if (SPLASH_SCREEN_FILE.exists()) {
            mSplashScreen.reset(
                new juce::SplashScreen("SpatGRIS3", juce::ImageFileFormat::loadFrom(SPLASH_SCREEN_FILE), true));
            mSplashScreen->deleteAfterDelay(juce::RelativeTime::seconds(4), false);
            mSplashScreen.release();
        }
#endif
    };

    auto const initAudioManager = [&]() {
        auto const & audioSettings{ mData.appData.audioSettings };
        AudioManager::init(audioSettings.deviceType,
                           audioSettings.inputDevice,
                           audioSettings.outputDevice,
                           audioSettings.sampleRate,
                           audioSettings.bufferSize);
    };

    auto const initAudioProcessor = [&]() {
        mAudioProcessor = std::make_unique<AudioProcessor>();
        juce::ScopedLock const audioLock{ mAudioProcessor->getLock() };
        auto & audioManager{ AudioManager::getInstance() };
        audioManager.registerAudioProcessor(mAudioProcessor.get());
        AudioManager::getInstance().getAudioDeviceManager().addChangeListener(this);
        audioParametersChanged();
        jassert(AudioManager::getInstance().getAudioDeviceManager().getCurrentAudioDevice());
    };

    auto const initGui = [&]() {
        // Set look and feel
        juce::LookAndFeel::setDefaultLookAndFeel(&grisLookAndFeel);

        // Create the menu bar.
        mMenuBar.reset(new juce::MenuBarComponent(this));
        addAndMakeVisible(mMenuBar.get());

        // SpeakerViewComponent 3D view
        mSpeakerViewComponent.reset(new SpeakerViewComponent(*this));
        addAndMakeVisible(mSpeakerViewComponent.get());

        // Box Main
        mMainLayout
            = std::make_unique<LayoutComponent>(LayoutComponent::Orientation::vertical, false, true, grisLookAndFeel);

        // info panel
        mInfoPanel = std::make_unique<InfoPanel>(*this, mLookAndFeel);

        // Source panel
        mSourcesLayout
            = std::make_unique<LayoutComponent>(LayoutComponent::Orientation::horizontal, true, false, grisLookAndFeel);
        mSourcesSection = std::make_unique<TitledComponent>("Inputs", mSourcesLayout.get(), mLookAndFeel);

        // Speaker panel
        mSpeakersLayout
            = std::make_unique<LayoutComponent>(LayoutComponent::Orientation::horizontal, true, false, grisLookAndFeel);
        mSpeakersSection = std::make_unique<TitledComponent>("Outputs", mSpeakersLayout.get(), mLookAndFeel);

        // Control panel
        mControlPanel = std::make_unique<ControlPanel>(*this, mLookAndFeel);
        mControlsSection = std::make_unique<TitledComponent>("Controls", mControlPanel.get(), mLookAndFeel);
        mControlPanel->setSpatMode(mData.speakerSetup.spatMode);
        mControlPanel->setCubeAttenuationDb(mData.project.lbapDistanceAttenuationData.attenuation);
        mControlPanel->setCubeAttenuationHz(mData.project.lbapDistanceAttenuationData.freq);
        mControlPanel->setStereoMode(mData.appData.stereoMode);

        mMainLayout->addSection(mInfoPanel.get()).withChildMinSize().withRightPadding(5);
        mMainLayout->addSection(mSourcesSection.get()).withRelativeSize(1.0f).withRightPadding(5);
        mMainLayout->addSection(mSpeakersSection.get()).withRelativeSize(1.0f).withRightPadding(5);
        mMainLayout->addSection(mControlsSection.get()).withChildMinSize().withBottomPadding(5).withRightPadding(5);

        addAndMakeVisible(mMainLayout.get());

        // Set up the layout and resize bars.
        mVerticalLayout.setItemLayout(0,
                                      -0.2,
                                      -0.8,
                                      -0.435); // width of the speaker view must be between 20% and 80%, preferably 50%
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

        // Restore last vertical divider position and speaker view cam distance.
        auto const sashPosition{ mData.appData.sashPosition };
        auto const trueSize{ narrow<int>(std::round(narrow<double>(getWidth() - 3) * std::abs(sashPosition))) };
        mVerticalLayout.setItemPosition(1, trueSize);

        mSpeakerViewComponent->setCameraPosition(mData.appData.cameraPosition);
    };

    auto const startOsc = [&]() {
        mOscReceiver.reset(new OscInput(*this));
        mOscReceiver->startConnection(mData.project.oscPort);
    };

    auto const initAppData = [&]() { mData.appData = mConfiguration.load(); };
    auto const initProject = [&]() {
        auto const success{
            loadProject(mData.appData.lastProject, LoadProjectOption::dontRemoveInvalidDirectOuts, true)
        };
        if (!success) {
            if (!loadProject(DEFAULT_PROJECT_FILE, LoadProjectOption::dontRemoveInvalidDirectOuts, true)) {
                fatalError("Unable to load the default project file.", this);
            }
        }
    };

    auto const initSpeakerSetup = [&]() {
        if (!loadSpeakerSetup(mData.appData.lastSpeakerSetup, LoadSpeakerSetupOption::allowDiscardingUnsavedChanges)) {
            if (!loadSpeakerSetup(DEFAULT_SPEAKER_SETUP_FILE, LoadSpeakerSetupOption::allowDiscardingUnsavedChanges)) {
                fatalError("Unable to load the default speaker setup", this);
            }
        }
    };

    auto const initCommandManager = [&]() {
        auto & commandManager{ mMainWindow.getApplicationCommandManager() };
        commandManager.registerAllCommandsForTarget(this);
    };

    // Load app config
    showSplashScreen();
    initAppData();
    initGui();
    initAudioManager();
    initAudioProcessor();

    juce::ScopedLock const audioLock{ mAudioProcessor->getLock() };

    initProject();
    initSpeakerSetup();
    startOsc();
    initCommandManager();

    // End layout and start refresh timer.
    resized();
    startTimerHz(24);
}

//==============================================================================
MainContentComponent::~MainContentComponent()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mOscReceiver.reset();

    {
        juce::ScopedWriteLock const lock{ mLock };

        auto const bounds{ MainWindow::getMainAppWindow()->DocumentWindow::getBounds() };
        mData.appData.windowX = bounds.getX();
        mData.appData.windowY = bounds.getY();
        mData.appData.windowWidth = bounds.getWidth();
        mData.appData.windowHeight = bounds.getHeight();
        mData.appData.sashPosition = mVerticalLayout.getItemCurrentRelativeSize(0);
        mData.appData.cameraPosition = mSpeakerViewComponent->getCameraPosition();

        mConfiguration.save(mData.appData);
    }

    mSpeakerViewComponent.reset();
}

//==============================================================================
juce::ApplicationCommandTarget * MainContentComponent::getNextCommandTarget()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return findFirstTargetParentComponent();
}

//==============================================================================
// Menu item action handlers.
void MainContentComponent::handleNewProject()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const success{ loadProject(DEFAULT_PROJECT_FILE, LoadProjectOption::removeInvalidDirectOuts, false) };
    if (!success) {
        fatalError("Unable to load the default project file.", this);
    }
}

//==============================================================================
bool MainContentComponent::loadProject(juce::File const & file,
                                       LoadProjectOption const loadProjectOption,
                                       bool const discardCurrentProject)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (!discardCurrentProject) {
        if (!makeSureProjectIsSavedToDisk()) {
            return false;
        }
    }

    auto const displayError = [&](juce::String const & error) {
        juce::AlertWindow::showNativeDialogBox("Unable to load SpatGRIS project.", error, false);
    };

    if (!file.existsAsFile()) {
        displayError("File \"" + file.getFullPathName() + "\" does not exists.");
        return false;
    }

    juce::XmlDocument xmlDoc{ file };
    auto const mainXmlElem{ xmlDoc.getDocumentElement() };

    if (!mainXmlElem) {
        displayError("File \"" + file.getFullPathName() + "\" is corrupted.\n" + xmlDoc.getLastParseError());
        return false;
    }

    if (mainXmlElem->hasTagName("SpeakerSetup") || mainXmlElem->hasTagName(SpeakerSetup::XmlTags::MAIN_TAG)) {
        // Wrong file type
        displayError("File \"" + file.getFullPathName() + "\" is a Speaker Setup, not a project.");
        return false;
    }

    auto projectData{ SpatGrisProjectData::fromXml(*mainXmlElem) };
    if (!projectData) {
        // Missing params
        displayError("File \"" + file.getFullPathName()
                     + "\" is missing one more mandatory parameters.\nYour file might be corrupted.");
        return false;
    }

    mData.project = std::move(*projectData);
    mData.appData.lastProject = file.getFullPathName();

    mControlPanel->setNumSources(mData.project.sources.size());
    mControlPanel->setMasterGain(mData.project.masterGain);
    mControlPanel->setInterpolation(mData.project.spatGainsInterpolation);

    if (loadProjectOption == LoadProjectOption::removeInvalidDirectOuts) {
        warnIfDirectOutMismatch();
    }

    updateAudioProcessor();
    updateViewportConfig();
    setTitle();
    refreshSourceVuMeterComponents();

    return true;
}

//==============================================================================
void MainContentComponent::handleOpenProject()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (!makeSureProjectIsSavedToDisk()) {
        return;
    }

    juce::FileChooser fc{ "Choose a file to open...", mData.appData.lastProject, "*.xml", true, false, this };

    if (!fc.browseForFileToOpen()) {
        return;
    }

    auto const chosen{ fc.getResult() };

    [[maybe_unused]] auto const success{ loadProject(chosen, LoadProjectOption::removeInvalidDirectOuts, true) };
    jassert(success);
}

//==============================================================================
void MainContentComponent::handleSaveProject()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    [[maybe_unused]] auto const success{ saveProject(mData.appData.lastProject) };
}

//==============================================================================
void MainContentComponent::handleSaveProjectAs()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    [[maybe_unused]] auto const success{ saveProject(tl::nullopt) };
}

//==============================================================================
void MainContentComponent::handleOpenSpeakerSetup()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    auto const initialFile{ mData.appData.lastSpeakerSetup };

    juce::FileChooser fc{ "Choose a file to open...", initialFile, "*.xml", true };

    if (!fc.browseForFileToOpen()) {
        return;
    }

    auto const chosen{ fc.getResult() };
    [[maybe_unused]] auto const success{ loadSpeakerSetup(chosen,
                                                          LoadSpeakerSetupOption::disallowDiscardingUnsavedChanges) };
}

//==============================================================================
void MainContentComponent::handleSaveSpeakerSetupAs()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    [[maybe_unused]] auto const success{ saveSpeakerSetup(tl::nullopt) };
}

//==============================================================================
void MainContentComponent::closeSpeakersConfigurationWindow()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mEditSpeakersWindow.reset();
}

//==============================================================================
void MainContentComponent::handleShowSpeakerEditWindow()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    juce::Rectangle<int> const result{ getScreenX() + mSpeakerViewComponent->getWidth() + 20,
                                       getScreenY() + 20,
                                       850,
                                       600 };
    if (mEditSpeakersWindow == nullptr) {
        auto const windowName = juce::String{ "Speakers Setup Edition - " }
                                + spatModeToString(mData.speakerSetup.spatMode) + " - "
                                + mData.appData.lastSpeakerSetup;
        mEditSpeakersWindow
            = std::make_unique<EditSpeakersWindow>(windowName, mLookAndFeel, *this, mData.appData.lastProject);
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
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    if (mPropertiesWindow == nullptr) {
        mPropertiesWindow.reset(new SettingsWindow{ *this, mData.project.oscPort, mLookAndFeel });
    }
}

//==============================================================================
void MainContentComponent::handleShow2DView()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

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
void MainContentComponent::handleShowAbout()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (!mAboutWindow) {
        mAboutWindow.reset(new AboutWindow{ "About SpatGRIS", mLookAndFeel, *this });
    }
}

//==============================================================================
void MainContentComponent::handleOpenManual()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (MANUAL_FILE.exists()) {
        juce::Process::openDocument("file:" + MANUAL_FILE.getFullPathName(), juce::String());
    }
}

//==============================================================================
void MainContentComponent::masterGainChanged(dbfs_t const gain)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.masterGain = gain;
    mControlPanel->setMasterGain(gain);

    updateAudioProcessor();
}

//==============================================================================
void MainContentComponent::interpolationChanged(float const interpolation)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.spatGainsInterpolation = interpolation;
    mControlPanel->setInterpolation(interpolation);

    updateAudioProcessor();
}

//==============================================================================
bool MainContentComponent::setSpatMode(SpatMode const spatMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto const ensureVbapIsDomeLike = [&]() {
        if (spatMode == SpatMode::vbap && !mData.speakerSetup.isDomeLike()) {
            auto const result{ juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::InfoIcon,
                "Converting to DOME",
                "A CUBE speaker setup will be converted to a DOME structure.\nThis will not affect the original file.",
                "Ok",
                "Cancel",
                this) };
            if (!result) {
                return false;
            }

            for (auto & node : mData.speakerSetup.speakers) {
                auto & speaker{ *node.value };
                if (speaker.isDirectOutOnly) {
                    continue;
                }
                speaker.vector = speaker.vector.normalized();
                speaker.position = speaker.vector.toCartesian();
            }
        }
        return true;
    };

    if (!ensureVbapIsDomeLike()) {
        mControlPanel->setSpatMode(mData.speakerSetup.spatMode);
        return false;
    }

    mData.speakerSetup.spatMode = spatMode;
    mControlPanel->setSpatMode(spatMode);
    return refreshSpeakers();
}

//==============================================================================
void MainContentComponent::setStereoMode(tl::optional<StereoMode> const stereoMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.appData.stereoMode = stereoMode;
    updateSpatAlgorithm();

    for (auto const & source : mData.project.sources) {
        if (!source.value->position) {
            continue;
        }

        updateSourceSpatData(source.key);
    }
}

//==============================================================================
void MainContentComponent::cubeAttenuationDbChanged(dbfs_t const value)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.lbapDistanceAttenuationData.attenuation = value;
    updateAudioProcessor();
}

//==============================================================================
void MainContentComponent::cubeAttenuationHzChanged(hz_t const value)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.lbapDistanceAttenuationData.freq = value;
    updateAudioProcessor();
}

//==============================================================================
void MainContentComponent::numSourcesChanged(int const numSources)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(numSources >= 1 && numSources <= MAX_NUM_SOURCES);
    juce::ScopedWriteLock const lock{ mLock };

    mControlPanel->setNumSources(numSources);

    if (numSources > mData.project.sources.size()) {
        source_index_t const firstNewIndex{ mData.project.sources.size() + 1 };
        source_index_t const lastNewIndex{ numSources };
        for (auto index{ firstNewIndex }; index <= lastNewIndex; ++index) {
            mData.project.sources.add(index, std::make_unique<SourceData>());
        }
    } else if (numSources < mData.project.sources.size()) {
        // remove some inputs
        while (mData.project.sources.size() > numSources) {
            source_index_t const index{ mData.project.sources.size() };
            mData.project.sources.remove(index);
        }
    }

    updateAudioProcessor();
    refreshSourceVuMeterComponents();
    unfocusAllComponents();
}

//==============================================================================
void MainContentComponent::recordButtonPressed()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    if (AudioManager::getInstance().isRecording()) {
        AudioManager::getInstance().stopRecording();
        mControlPanel->setRecordButtonState(RecordButton::State::ready);
        return;
    }

    if (mPrepareToRecordWindow) {
        return;
    }

    mPrepareToRecordWindow = std::make_unique<PrepareToRecordWindow>(mData.appData.lastRecordingDirectory,
                                                                     mData.appData.recordingOptions,
                                                                     *this,
                                                                     mLookAndFeel);
}

//==============================================================================
void MainContentComponent::handleShowNumbers()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSpeakerNumbers };
    var = !var;
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSpeakers()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSpeakers };
    var = !var;
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowTriplets()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const readLock{ mLock };

    auto const newState{ !mData.appData.viewSettings.showSpeakerTriplets };
    if (newState && !mAudioProcessor->getSpatAlgorithm()->hasTriplets()) {
        jassertfalse;
        return;
    }

    juce::ScopedWriteLock const writeLock{ mLock };
    mData.appData.viewSettings.showSpeakerTriplets = newState;
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSourceLevel()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSourceActivity };
    var = !var;
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSpeakerLevel()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSpeakerLevels };
    var = !var;
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSphere()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSphereOrCube };
    var = !var;
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleResetInputPositions()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    jassertfalse; // TODO

    // juce::ScopedWriteLock const lock{ mLock };

    // auto & viewPortData{ mSpeakerViewComponent->getData() };
    // auto & spatDataExchangers{ mAudioProcessor->getAudioData().spatData };
    // for (auto const source : mData.project.sources) {
    //    // reset positions
    //    source.value->position = tl::nullopt;
    //    source.value->vector = tl::nullopt;

    //    {
    //        // reset 3d view
    //        auto & exchanger{ viewPortData.sources[source.key] };
    //        auto * sourceTicket{ exchanger.acquire() };
    //        sourceTicket->get() = tl::nullopt;
    //        exchanger.setMostRecent(sourceTicket);
    //    }
    //    {
    //        // reset gain matrix
    //        auto & exchanger{ spatDataExchangers[source.key] };
    //        auto * spatData{ exchanger.acquire() };
    //        spatData->get() = SourceSpatData{};
    //        exchanger.setMostRecent(spatData);
    //    }
    //}

    // updateAudioProcessor();
}

//==============================================================================
void MainContentComponent::handleResetMeterClipping()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    for (auto vuMeter : mSourceVuMeterComponents) {
        vuMeter.value->resetClipping();
    }
    for (auto vuMeter : mSpeakerVuMeterComponents) {
        vuMeter.value->resetClipping();
    }
}

//==============================================================================
void MainContentComponent::handleColorizeInputs()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

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
    JUCE_ASSERT_MESSAGE_THREAD;

    // this returns the set of all commands that this target can perform.
    const juce::CommandID ids[] = {
        MainWindow::CommandIDs::NewProjectID,
        MainWindow::CommandIDs::OpenProjectID,
        MainWindow::CommandIDs::SaveProjectID,
        MainWindow::CommandIDs::SaveAsProjectID,
        MainWindow::CommandIDs::OpenSpeakerSetupID,
        MainWindow::CommandIDs::SaveSpeakerSetupID,
        MainWindow::CommandIDs::SaveSpeakerSetupAsID,
        MainWindow::CommandIDs::ShowSpeakerEditID,
        MainWindow::CommandIDs::Show2DViewID,
        MainWindow::CommandIDs::ShowNumbersID,
        MainWindow::CommandIDs::ShowSpeakersID,
        MainWindow::CommandIDs::ShowTripletsID,
        MainWindow::CommandIDs::ShowSourceLevelID,
        MainWindow::CommandIDs::ShowSpeakerLevelID,
        MainWindow::CommandIDs::ShowSphereID,
        MainWindow::CommandIDs::ColorizeInputsID,
        MainWindow::CommandIDs::ResetInputPosID,
        MainWindow::CommandIDs::ResetMeterClipping,
        MainWindow::CommandIDs::OpenSettingsWindowID,
        MainWindow::CommandIDs::QuitID,
        MainWindow::CommandIDs::AboutID,
        MainWindow::CommandIDs::OpenManualID,
    };

    commands.addArray(ids, juce::numElementsInArray(ids));
}

//==============================================================================
void MainContentComponent::getCommandInfo(juce::CommandID const commandId, juce::ApplicationCommandInfo & result)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

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
        result.setActive(isProjectModified());
        break;
    case MainWindow::SaveAsProjectID:
        result.setInfo("Save Project As...", "Save the current project under a new name on disk.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::shiftModifier | juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::OpenSpeakerSetupID:
        result.setInfo("Load Speaker Setup", "Choose a new speaker setup on disk.", generalCategory, 0);
        result.addDefaultKeypress('L', juce::ModifierKeys::commandModifier);
        break;
    case MainWindow::SaveSpeakerSetupID:
        result.setInfo("Save Speaker Setup", "Save the current speaker setup on disk.", generalCategory, 0);
        result.setActive(isSpeakerSetupModified());
        break;
    case MainWindow::SaveSpeakerSetupAsID:
        result.setInfo("Save Speaker Setup As...",
                       "Save the current speaker setup under a new name on disk.",
                       generalCategory,
                       0);
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
        result.setTicked(mData.appData.viewSettings.showSpeakerNumbers);
        break;
    case MainWindow::ShowSpeakersID:
        result.setInfo("Show Speakers", "Show speakers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSpeakers);
        break;
    case MainWindow::ShowTripletsID:
        result.setInfo("Show Speaker Triplets", "Show speaker triplets on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('T', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSpeakerTriplets);
        result.setActive(mAudioProcessor->getSpatAlgorithm()->hasTriplets());
        break;
    case MainWindow::ShowSourceLevelID:
        result.setInfo("Show Source Activity", "Activate brightness on sources on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('A', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSourceActivity);
        break;
    case MainWindow::ShowSpeakerLevelID:
        result.setInfo("Show Speaker Level", "Activate brightness on speakers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('L', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSpeakerLevels);
        break;
    case MainWindow::ShowSphereID:
        result.setInfo("Show Sphere/Cube", "Show the sphere on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('O', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSphereOrCube);
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
    JUCE_ASSERT_MESSAGE_THREAD;

    if (MainWindow::getMainAppWindow()) {
        switch (info.commandID) {
        case MainWindow::NewProjectID:
            handleNewProject();
            break;
        case MainWindow::OpenProjectID:
            handleOpenProject();
            break;
        case MainWindow::SaveProjectID:
            handleSaveProject();
            break;
        case MainWindow::SaveAsProjectID:
            handleSaveProjectAs();
            break;
        case MainWindow::OpenSpeakerSetupID:
            handleOpenSpeakerSetup();
            break;
        case MainWindow::SaveSpeakerSetupID:
            handleSaveSpeakerSetup();
            break;
        case MainWindow::SaveSpeakerSetupAsID:
            handleSaveSpeakerSetupAs();
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
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedLock const audioLock{ mAudioProcessor->getLock() };

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

    juce::ScopedWriteLock const lock{ mLock };

    mData.appData.audioSettings.sampleRate = setup.sampleRate;
    mData.appData.audioSettings.bufferSize = setup.bufferSize;
    mData.appData.audioSettings.deviceType = deviceTypeName;
    mData.appData.audioSettings.inputDevice = setup.inputDeviceName;
    mData.appData.audioSettings.outputDevice = setup.outputDeviceName;

    AudioManager::getInstance().setBufferSize(bufferSize);

    mInfoPanel->setSampleRate(sampleRate);
    mInfoPanel->setBufferSize(bufferSize);
    mInfoPanel->setNumInputs(inputCount);
    mInfoPanel->setNumOutputs(outputCount);
}

//==============================================================================
juce::PopupMenu MainContentComponent::getMenuForIndex(int /*menuIndex*/, const juce::String & menuName)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    juce::ApplicationCommandManager * commandManager = &mMainWindow.getApplicationCommandManager();

    juce::PopupMenu menu;

    if (menuName == "File") {
        menu.addCommandItem(commandManager, MainWindow::NewProjectID);
        menu.addCommandItem(commandManager, MainWindow::OpenProjectID);
        menu.addCommandItem(commandManager, MainWindow::SaveProjectID);
        menu.addCommandItem(commandManager, MainWindow::SaveAsProjectID);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::OpenSpeakerSetupID);
        menu.addCommandItem(commandManager, MainWindow::SaveSpeakerSetupID);
        menu.addCommandItem(commandManager, MainWindow::SaveSpeakerSetupAsID);
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
        if (mAudioProcessor->getSpatAlgorithm()->hasTriplets()) {
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
    JUCE_ASSERT_MESSAGE_THREAD;
}

//==============================================================================
// Exit functions.
bool MainContentComponent::isProjectModified() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    auto const savedElement{ juce::XmlDocument{ juce::File{ mData.appData.lastProject } }.getDocumentElement() };
    jassert(savedElement);
    if (!savedElement) {
        return true;
    }
    auto const savedProject{ SpatGrisProjectData::fromXml(*savedElement) };
    jassert(savedProject);
    if (!savedProject) {
        return true;
    }
    return mData.project != *savedProject;
}

//==============================================================================
bool MainContentComponent::isSpeakerSetupModified() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    auto const savedElement{ juce::XmlDocument{ juce::File{ mData.appData.lastSpeakerSetup } }.getDocumentElement() };
    jassert(savedElement);
    if (!savedElement) {
        return true;
    }

    auto const savedSpeakerSetup{ SpeakerSetup::fromXml(*savedElement) };
    jassert(savedSpeakerSetup);
    if (!savedSpeakerSetup) {
        return true;
    }
    return mData.speakerSetup != *savedSpeakerSetup;
}

//==============================================================================
bool MainContentComponent::exitApp()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    static constexpr auto EXIT_APP = true;
    static constexpr auto DONT_EXIT_APP = false;

    if (!makeSureSpeakerSetupIsSavedToDisk()) {
        return DONT_EXIT_APP;
    }

    if (!makeSureProjectIsSavedToDisk()) {
        return DONT_EXIT_APP;
    }

    return EXIT_APP;
}

//==============================================================================
void MainContentComponent::updatePeaks()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    auto & viewportData{ mSpeakerViewComponent->getData() };

    auto & audioData{ mAudioProcessor->getAudioData() };
    auto *& sourcePeaksTicket{ mData.mostRecentSourcePeaks };
    audioData.sourcePeaks.getMostRecent(sourcePeaksTicket);
    if (!sourcePeaksTicket) {
        return;
    }
    auto const & sourcePeaks{ sourcePeaksTicket->get() };
    for (auto const sourceData : mData.project.sources) {
        auto const & peak{ sourcePeaks[sourceData.key] };
        auto const dbPeak{ dbfs_t::fromGain(peak) };
        mSourceVuMeterComponents[sourceData.key].setLevel(dbPeak);

        if (!sourceData.value->position) {
            continue;
        }
        if (!viewportData.sources.contains(sourceData.key)) {
            viewportData.sources.add(sourceData.key);
        }

        auto & exchanger{ viewportData.sources[sourceData.key] };
        auto * ticket{ exchanger.acquire() };
        ticket->get() = sourceData.value->toViewportData(
            mData.appData.viewSettings.showSourceActivity ? gainToSourceAlpha(peak) : 0.8f);
        exchanger.setMostRecent(ticket);
    }

    auto *& speakerPeaksTicket{ mData.mostRecentSpeakerPeaks };
    audioData.speakerPeaks.getMostRecent(speakerPeaksTicket);
    if (speakerPeaksTicket == nullptr) {
        return;
    }
    auto const & speakerPeaks{ speakerPeaksTicket->get() };
    for (auto const speaker : mData.speakerSetup.speakers) {
        auto const & peak{ speakerPeaks[speaker.key] };
        auto const dbPeak{ dbfs_t::fromGain(peak) };
        mSpeakerVuMeterComponents[speaker.key].setLevel(dbPeak);

        auto & exchanger{ viewportData.speakersAlpha[speaker.key] };
        auto * ticket{ exchanger.acquire() };
        ticket->get() = gainToSpeakerAlpha(peak);
        exchanger.setMostRecent(ticket);
    }
}

//==============================================================================
void MainContentComponent::refreshSourceVuMeterComponents()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    mSourcesLayout->clear();
    mSourceVuMeterComponents.clear();

    // auto x{ 3 };
    for (auto source : mData.project.sources) {
        auto newVuMeter{ std::make_unique<SourceVuMeterComponent>(source.key,
                                                                  source.value->directOut,
                                                                  source.value->colour,
                                                                  *this,
                                                                  mSmallLookAndFeel) };
        auto & addedVuMeter{ mSourceVuMeterComponents.add(source.key, std::move(newVuMeter)) };
        mSourcesLayout->addSection(&addedVuMeter).withChildMinSize();
    }

    mSourcesLayout->resized();
}

//==============================================================================
void MainContentComponent::refreshSpeakerVuMeterComponents()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    mSpeakersLayout->clear();
    mSpeakerVuMeterComponents.clear();

    // auto x{ 3 };
    for (auto const outputPatch : mData.speakerSetup.order) {
        auto newVuMeter{ std::make_unique<SpeakerVuMeterComponent>(outputPatch, *this, mSmallLookAndFeel) };
        auto & addedVuMeter{ mSpeakerVuMeterComponents.add(outputPatch, std::move(newVuMeter)) };
        mSpeakersLayout->addSection(&addedVuMeter).withChildMinSize();
    }

    mSpeakersLayout->resized();
}

//==============================================================================
void MainContentComponent::updateSourceSpatData(source_index_t const sourceIndex)
{
    jassert(!isProbablyAudioThread());

    juce::ScopedReadLock const lock{ mLock };
    mAudioProcessor->getSpatAlgorithm()->updateSpatData(sourceIndex, mData.project.sources[sourceIndex]);
}

//==============================================================================
void MainContentComponent::warnIfDirectOutMismatch()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    static bool errorShown{};

    for (auto & source : mData.project.sources) {
        auto & directOut{ source.value->directOut };
        if (!directOut) {
            continue;
        }

        if (mData.speakerSetup.speakers.contains(*directOut)) {
            continue;
        }

        if (errorShown) {
            return;
        }

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::AlertIconType::WarningIcon,
            "Direct Out Mismatch",
            "Some of your selected direct out channels don't exist in the current speaker setup.",
            "Ok",
            this);
        errorShown = true;
        return;
    }

    errorShown = false;
}

//==============================================================================
void MainContentComponent::handleSourcePositionChanged(source_index_t const sourceIndex,
                                                       radians_t const azimuth,
                                                       radians_t const elevation,
                                                       float const length,
                                                       float const newAzimuthSpan,
                                                       float const newZenithSpan)
{
    ASSERT_OSC_THREAD;
    juce::ScopedReadLock const readLock{ mLock };

    if (!mData.project.sources.contains(sourceIndex)) {
        return;
    }

    auto const getCorrectPosition = [&]() {
        switch (mData.speakerSetup.spatMode) {
        case SpatMode::vbap:
            return PolarVector{ azimuth, elevation, length }.normalized();
        case SpatMode::lbap: {
            return LegacyLbapPosition{ azimuth, elevation, length }.toPolar();
        }
        }
        jassertfalse;
        return PolarVector{};
    };

    auto const correctedPosition{ getCorrectPosition() };
    auto & source{ mData.project.sources[sourceIndex] };

    if (correctedPosition == source.vector && newAzimuthSpan == source.azimuthSpan
        && newZenithSpan == source.zenithSpan) {
        return;
    }

    juce::ScopedWriteLock const writeLock{ mLock };

    source.vector = correctedPosition;
    source.position = source.vector->toCartesian();
    source.azimuthSpan = newAzimuthSpan;
    source.zenithSpan = newZenithSpan;

    updateSourceSpatData(sourceIndex);
}

//==============================================================================
void MainContentComponent::resetSourcePosition(source_index_t const sourceIndex)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.sources[sourceIndex].position = tl::nullopt;
    mData.project.sources[sourceIndex].vector = tl::nullopt;
}

//==============================================================================
void MainContentComponent::setRecordingFormat(RecordingFormat const format)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.appData.recordingOptions.format = format;
}

//==============================================================================
void MainContentComponent::setRecordingFileType(RecordingFileType const fileType)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.appData.recordingOptions.fileType = fileType;
}

//==============================================================================
void MainContentComponent::handleSpeakerOnlyDirectOutChanged(output_patch_t const outputPatch, bool const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & val{ mData.speakerSetup.speakers[outputPatch].isDirectOutOnly };
    if (state != val) {
        val = state;
        updateAudioProcessor();
        updateViewportConfig();
    }
}

//==============================================================================
void MainContentComponent::handleSpeakerOutputPatchChanged(output_patch_t const oldOutputPatch,
                                                           output_patch_t const newOutputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & speakers{ mData.speakerSetup.speakers };
    speakers.add(newOutputPatch, std::make_unique<SpeakerData>(speakers[oldOutputPatch]));
    speakers.remove(oldOutputPatch);

    auto & order{ mData.speakerSetup.order };
    order.set(order.indexOf(oldOutputPatch), newOutputPatch);

    refreshSpeakerVuMeterComponents();
}

//==============================================================================
void MainContentComponent::handleSetSpeakerGain(output_patch_t const outputPatch, dbfs_t const gain)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.speakerSetup.speakers[outputPatch].gain = gain;

    updateAudioProcessor();
}

//==============================================================================
void MainContentComponent::handleSetSpeakerHighPassFreq(output_patch_t const outputPatch, hz_t const freq)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (freq == hz_t{ 0.0f }) {
        mData.speakerSetup.speakers[outputPatch].highpassData.reset();
    } else {
        mData.speakerSetup.speakers[outputPatch].highpassData = SpeakerHighpassData{ freq };
    }
}

//==============================================================================
void MainContentComponent::handlePinkNoiseGainChanged(tl::optional<dbfs_t> const gain)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (gain != mData.pinkNoiseLevel) {
        mData.pinkNoiseLevel = gain;
        mAudioProcessor->setAudioConfig(mData.toAudioConfig());
    }
}

//==============================================================================
void MainContentComponent::handleSourceColorChanged(source_index_t const sourceIndex, juce::Colour const colour)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.sources[sourceIndex].colour = colour;
    mSourceVuMeterComponents[sourceIndex].setSourceColour(colour);
}

//==============================================================================
void MainContentComponent::handleSourceStateChanged(source_index_t const sourceIndex, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.sources[sourceIndex].state = state;

    auto const isAtLeastOneSourceSolo{ std::any_of(
        mData.project.sources.cbegin(),
        mData.project.sources.cend(),
        [](SourcesData::ConstNode const & node) { return node.value->state == PortState::solo; }) };

    updateAudioProcessor();
    mSourceVuMeterComponents[sourceIndex].setState(state, isAtLeastOneSourceSolo);
}

//==============================================================================
void MainContentComponent::handleSpeakerSelected(juce::Array<output_patch_t> const selection)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    for (auto const speaker : mData.speakerSetup.speakers) {
        auto const isSelected{ selection.contains(speaker.key) };

        if (speaker.value->isSelected == isSelected) {
            continue;
        }

        speaker.value->isSelected = isSelected;
        mSpeakerVuMeterComponents[speaker.key].setSelected(isSelected);

        if (!isSelected || !mEditSpeakersWindow) {
            continue;
        }

        mEditSpeakersWindow->selectSpeaker(speaker.key);
    }

    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleSpeakerStateChanged(output_patch_t const outputPatch, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.speakerSetup.speakers[outputPatch].state = state;
    updateAudioProcessor();

    auto const isAtLeastOneSpeakerSolo{ std::any_of(
        mData.speakerSetup.speakers.cbegin(),
        mData.speakerSetup.speakers.cend(),
        [](SpeakersData::ConstNode const & node) { return node.value->state == PortState::solo; }) };

    mSpeakerVuMeterComponents[outputPatch].setState(state, isAtLeastOneSpeakerSolo);
    // TODO : update 3D view ?
}

//==============================================================================
void MainContentComponent::handleSourceDirectOutChanged(source_index_t const sourceIndex,
                                                        tl::optional<output_patch_t> const outputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.sources[sourceIndex].directOut = outputPatch;

    if (mData.appData.stereoMode) {
        refreshSpeakers();
    } else {
        updateAudioProcessor();
    }

    mSourceVuMeterComponents[sourceIndex].setDirectOut(outputPatch);
}

//==============================================================================
void MainContentComponent::handleNewSpeakerPosition(output_patch_t const outputPatch, CartesianVector const & position)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & speaker{ mData.speakerSetup.speakers[outputPatch] };
    speaker.vector = PolarVector::fromCartesian(position);
    speaker.position = position;

    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::handleNewSpeakerPosition(output_patch_t const outputPatch, PolarVector const & position)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & speaker{ mData.speakerSetup.speakers[outputPatch] };
    speaker.vector = position;
    speaker.position = position.toCartesian();

    // TODO : re-init spat algorithm?
}

//==============================================================================
void MainContentComponent::updateAudioProcessor() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    mAudioProcessor->setAudioConfig(mData.toAudioConfig());
}

//==============================================================================
void MainContentComponent::updateSpatAlgorithm() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };
    juce::ScopedLock const audioLock{ mAudioProcessor->getLock() };

    mAudioProcessor->getSpatAlgorithm()
        = AbstractSpatAlgorithm::make(mData.speakerSetup, mData.appData.stereoMode, mData.project.sources);
    updateAudioProcessor();
}

//==============================================================================
void MainContentComponent::updateViewportConfig() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    auto const & spatAlgorithm{ *mAudioProcessor->getSpatAlgorithm() };

    auto const newConfig{ mData.toViewportConfig() };
    if (newConfig.viewSettings.showSpeakerTriplets && spatAlgorithm.hasTriplets()) {
        mSpeakerViewComponent->setTriplets(spatAlgorithm.getTriplets());
    }
    mSpeakerViewComponent->setConfig(newConfig, mData.project.sources);
}

//==============================================================================
void MainContentComponent::handleSetShowTriplets(bool const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.appData.viewSettings.showSpeakerTriplets = state;
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::setSourceState(source_index_t const sourceIndex, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.sources[sourceIndex].state = state;
    updateAudioProcessor();
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::setSpeakerState(output_patch_t const outputPatch, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.speakerSetup.speakers[outputPatch].state = state;
    updateAudioProcessor();
    updateViewportConfig();
}

//==============================================================================
void MainContentComponent::reorderSpeakers(juce::Array<output_patch_t> newOrder)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & order{ mData.speakerSetup.order };
    jassert(newOrder.size() == order.size());
    order = std::move(newOrder);
    refreshSpeakerVuMeterComponents();
}

//==============================================================================
output_patch_t MainContentComponent::getMaxSpeakerOutputPatch() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(!mData.speakerSetup.order.isEmpty());
    juce::ScopedReadLock const lock{ mLock };

    auto const & patches{ mData.speakerSetup.order };
    auto const maxPatch{ *std::max_element(patches.begin(), patches.end()) };
    return maxPatch;
}

//==============================================================================
tl::optional<SpeakerSetup> MainContentComponent::extractSpeakerSetup(juce::File const & file)
{
    auto const displayError = [&](juce::String const & message) {
        juce::AlertWindow::showNativeDialogBox("Unable to load Speaker Setup.", message, false);
    };

    if (!file.existsAsFile()) {
        displayError("File \"" + file.getFullPathName() + "\" does not exist.");
        return tl::nullopt;
    }

    juce::XmlDocument xmlDoc{ file };
    auto const mainXmlElem(xmlDoc.getDocumentElement());
    if (!mainXmlElem) {
        displayError("Corrupted file.\n" + xmlDoc.getLastParseError());
        return tl::nullopt;
    }

    if (mainXmlElem->hasTagName("ServerGRIS_Preset")
        || mainXmlElem->hasTagName(SpatGrisProjectData::XmlTags::MAIN_TAG)) {
        displayError("This is a project file, not a Speaker Setup !");
        return tl::nullopt;
    }

    auto speakerSetup{ SpeakerSetup::fromXml(*mainXmlElem) };

    if (!speakerSetup) {
        displayError("Error while reading file.");
    }

    return speakerSetup;
}

//==============================================================================
output_patch_t MainContentComponent::addSpeaker(tl::optional<output_patch_t> const speakerToCopy,
                                                tl::optional<int> const index)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto newSpeaker{ std::make_unique<SpeakerData>() };

    if (speakerToCopy) {
        auto const speakerToCopyExists{ mData.speakerSetup.speakers.contains(*speakerToCopy) };
        jassert(speakerToCopyExists);
        if (speakerToCopyExists) {
            *newSpeaker = mData.speakerSetup.speakers[*speakerToCopy];
        }
    }

    auto const newOutputPatch{ ++getMaxSpeakerOutputPatch() };

    if (index) {
        auto const isValidIndex{ *index >= 0 && *index < mData.speakerSetup.order.size() };
        jassert(isValidIndex);
        if (isValidIndex) {
            mData.speakerSetup.order.insert(*index, newOutputPatch);
        }
    } else {
        mData.speakerSetup.order.add(newOutputPatch);
    }

    mData.speakerSetup.speakers.add(newOutputPatch, std::move(newSpeaker));

    // refreshSpeakerVuMeterComponents();
    // updateViewportConfig();

    refreshSpeakers();

    return newOutputPatch;
}

//==============================================================================
void MainContentComponent::removeSpeaker(output_patch_t const outputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.speakerSetup.order.removeFirstMatchingValue(outputPatch);
    mData.speakerSetup.speakers.remove(outputPatch);

    warnIfDirectOutMismatch();
    updateViewportConfig();
    updateAudioProcessor();
}

//==============================================================================
bool MainContentComponent::refreshSpeakers()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    warnIfDirectOutMismatch();

    auto const & speakers{ mData.speakerSetup.speakers };
    auto const numActiveSpeakers{ std::count_if(
        speakers.cbegin(),
        speakers.cend(),
        [](SpeakersData::ConstNode const speaker) { return !speaker.value->isDirectOutOnly; }) };

    // Ensure there is enough speakers
    auto const showNotEnoughSpeakersError = [&]() {
        juce::AlertWindow alert("Not enough speakers !    ",
                                "Do you want to reload the default setup ?    ",
                                juce::AlertWindow::WarningIcon);
        alert.setLookAndFeel(&mLookAndFeel);
        alert.addButton("No", 0);
        alert.addButton("Yes", 1, juce::KeyPress(juce::KeyPress::returnKey));
        if (alert.runModalLoop() != 0) {
            auto const success{ loadSpeakerSetup(DEFAULT_SPEAKER_SETUP_FILE,
                                                 LoadSpeakerSetupOption::allowDiscardingUnsavedChanges) };
            if (!success) {
                fatalError("Unable to load the default speaker setup.", this);
            }
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
                                                       [&](SpeakersData::ConstNode const node) {
                                                           auto const zenith{ node.value->vector.elevation };
                                                           return zenith < maxZenith && zenith > minZenith;
                                                       }) };
        return areSpeakersOnSamePlane ? VbapType::twoD : VbapType::threeD;
    };

    auto const lbapDimensions{ getVbapDimensions() };
    if (lbapDimensions == VbapType::twoD) {
        mData.appData.viewSettings.showSpeakerTriplets = false;
    } else if (mData.speakerSetup.speakers.size() < 3) {
        showNotEnoughSpeakersError();
        return false;
    }

    updateSpatAlgorithm();

    // re-assign source positions
    for (auto const & source : mData.project.sources) {
        if (!source.value->position) {
            continue;
        }
        updateSourceSpatData(source.key);
    }

    refreshSpeakerVuMeterComponents();
    updateViewportConfig();
    if (mEditSpeakersWindow != nullptr) {
        mEditSpeakersWindow->updateWinContent();
    }

    return true;
}

//==============================================================================
bool MainContentComponent::loadSpeakerSetup(juce::File const file, LoadSpeakerSetupOption const option)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto speakerSetup{ extractSpeakerSetup(file) };

    if (!speakerSetup) {
        return false;
    }

    if (option == LoadSpeakerSetupOption::disallowDiscardingUnsavedChanges) {
        if (!makeSureSpeakerSetupIsSavedToDisk()) {
            return false;
        }
    }

    juce::ScopedWriteLock const lock{ mLock };

    mData.speakerSetup = std::move(*speakerSetup);
    mData.appData.lastSpeakerSetup = file.getFullPathName();

    // specific mode-dependent checks
    switch (mData.speakerSetup.spatMode) {
    case SpatMode::vbap:
        mData.appData.viewSettings.showSpeakerTriplets = false;
        break;
    case SpatMode::lbap:
        mControlPanel->setCubeAttenuationDb(mData.project.lbapDistanceAttenuationData.attenuation);
        mControlPanel->setCubeAttenuationHz(mData.project.lbapDistanceAttenuationData.freq);
        break;
    default:
        jassertfalse;
        break;
    }

    refreshSpeakers();

    mControlPanel->setSpatMode(mData.speakerSetup.spatMode);

    if (mEditSpeakersWindow != nullptr) {
        auto const windowName{ juce::String{ "Speakers Setup Edition - " }
                               + spatModeToString(mData.speakerSetup.spatMode) + juce::String(" - ")
                               + mData.appData.lastSpeakerSetup };
        mEditSpeakersWindow->setName(windowName);
    }

    return true;
}

//==============================================================================
void MainContentComponent::setTitle() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    juce::File const currentProject{ mData.appData.lastProject };
    auto const title{ juce::String{ "SpatGRIS v" } + juce::JUCEApplication::getInstance()->getApplicationVersion()
                      + " - " + currentProject.getFileName() };
    mMainWindow.DocumentWindow::setName(title);
}

//==============================================================================
bool MainContentComponent::performSafeSave(juce::XmlElement const & content, juce::File const & destination) noexcept
{
    if (destination == DEFAULT_PROJECT_FILE || destination == DEFAULT_SPEAKER_SETUP_FILE
        || destination == BINAURAL_SPEAKER_SETUP_FILE || destination == STEREO_SPEAKER_SETUP_FILE) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Error",
                                               "The file your are trying to save to is a default SpatGRIS file and "
                                               "cannot be overridden. Please select an other location to save to.",
                                               "",
                                               this);
        return false;
    }
    auto const success{ content.writeTo(destination) };
    if (!success) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Error",
            "SpatGRIS was unable to save the project file to the specified location. Make sure that this location "
            "exists and that the current user has access to it.",
            "",
            this);
    }
    return success;
}

//==============================================================================
void MainContentComponent::handleTimer(bool const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (state) {
        startTimerHz(24);
    } else {
        stopTimer();
    }
}

//==============================================================================
void MainContentComponent::handleSaveSpeakerSetup()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    [[maybe_unused]] auto const success{ saveSpeakerSetup(mData.appData.lastSpeakerSetup) };
    jassert(success);
}

//==============================================================================
bool MainContentComponent::saveProject(tl::optional<juce::File> maybeFile)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (!maybeFile) {
        juce::File const lastProjectFile{ mData.appData.lastProject };
        auto const initialFile{ lastProjectFile == DEFAULT_PROJECT_FILE ? juce::File::getSpecialLocation(
                                    juce::File::SpecialLocationType::userDesktopDirectory)
                                                                        : lastProjectFile };
        juce::FileChooser fc{ "Choose file to save to...", initialFile, "*.xml", true, false, this };
        if (!fc.browseForFileToSave(true)) {
            return false;
        }

        maybeFile = fc.getResult();
    }

    auto const file{ *maybeFile };

    auto const success{ performSafeSave(*mData.project.toXml(), file) };
    jassert(success);

    if (success) {
        mData.appData.lastProject = maybeFile->getFullPathName();
    }

    setTitle();

    return success;
}

//==============================================================================
bool MainContentComponent::saveSpeakerSetup(tl::optional<juce::File> maybeFile)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (!maybeFile) {
        juce::File const lastSpeakerSetup{ mData.appData.lastSpeakerSetup };
        auto const initialFile{ lastSpeakerSetup == DEFAULT_SPEAKER_SETUP_FILE ? juce::File::getSpecialLocation(
                                    juce::File::SpecialLocationType::userDesktopDirectory)
                                                                               : mData.appData.lastSpeakerSetup };
        juce::FileChooser fc{ "Choose file to save to...", initialFile, "*.xml", true, false, this };
        if (!fc.browseForFileToSave(true)) {
            return false;
        }

        maybeFile = fc.getResult();
    }

    auto const & file{ *maybeFile };
    auto const content{ mData.speakerSetup.toXml() };

    auto const success{ performSafeSave(*content, file) };
    jassert(success);

    if (success) {
        mData.appData.lastSpeakerSetup = maybeFile->getFullPathName();
    }

    return success;
}

//==============================================================================
bool MainContentComponent::makeSureProjectIsSavedToDisk() noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (!isProjectModified()) {
        return true;
    }

    juce::AlertWindow alertWindow{ "Unsaved project",
                                   "Your current project has unsaved changes. Do you wish to save it to disk?",
                                   juce::AlertWindow::AlertIconType::InfoIcon,
                                   this };
    alertWindow.addButton("Save", BUTTON_OK, juce::KeyPress{ juce::KeyPress::returnKey });
    alertWindow.addButton("Discard", BUTTON_DISCARD);
    alertWindow.addButton("Cancel", BUTTON_CANCEL, juce::KeyPress{ juce::KeyPress::escapeKey });

    auto const pressedButton{ alertWindow.runModalLoop() };

    if (pressedButton == BUTTON_CANCEL) {
        return false;
    }

    if (pressedButton == BUTTON_DISCARD) {
        return true;
    }

    jassert(pressedButton == BUTTON_OK);

    return saveProject(tl::nullopt);
}

//==============================================================================
bool MainContentComponent::makeSureSpeakerSetupIsSavedToDisk() noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (!isSpeakerSetupModified()) {
        return true;
    }

    juce::AlertWindow alertWindow{ "Unsaved speaker setup",
                                   "Your current speaker setup has unsaved changes. Do you wish to save it to disk?",
                                   juce::AlertWindow::AlertIconType::InfoIcon,
                                   this };
    alertWindow.addButton("Save", BUTTON_OK, juce::KeyPress{ juce::KeyPress::returnKey });
    alertWindow.addButton("Discard", BUTTON_DISCARD);
    alertWindow.addButton("Cancel", BUTTON_CANCEL, juce::KeyPress{ juce::KeyPress::escapeKey });

    auto const pressedButton{ alertWindow.runModalLoop() };

    if (pressedButton == BUTTON_CANCEL) {
        return false;
    }

    if (pressedButton == BUTTON_DISCARD) {
        return true;
    }

    jassert(pressedButton == BUTTON_OK);

    return saveSpeakerSetup(tl::nullopt);
}

//==============================================================================
void MainContentComponent::timerCallback()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    // Update levels
    updatePeaks();

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

    mInfoPanel->setCpuLoad(cpuRunningAverage);

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
    JUCE_ASSERT_MESSAGE_THREAD;

    g.fillAll(mLookAndFeel.getWinBackgroundColour());
}

//==============================================================================
juce::StringArray MainContentComponent::getMenuBarNames()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    char const * names[] = { "File", "View", "Help", nullptr };
    return juce::StringArray{ names };
}

//==============================================================================
void MainContentComponent::prepareAndStartRecording(juce::File const & fileOrDirectory,
                                                    RecordingOptions const & recordingOptions)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    {
        juce::ScopedWriteLock const lock{ mLock };

        mData.appData.lastRecordingDirectory = fileOrDirectory.getParentDirectory().getFullPathName();
        mData.appData.recordingOptions = recordingOptions;
    }

    juce::ScopedReadLock const lock{ mLock };
    auto const getSpeakersToRecord = [&]() {
        juce::Array<output_patch_t> result{};

        if (mData.appData.stereoMode) {
            result.add(output_patch_t{ 1 });
            result.add(output_patch_t{ 2 });
            return result;
        }

        result = mData.speakerSetup.order;
        result.sort();

        return result;
    };

    AudioManager::RecordingParameters const recordingParams{ fileOrDirectory.getFullPathName(),
                                                             mData.appData.recordingOptions,
                                                             mData.appData.audioSettings.sampleRate,
                                                             getSpeakersToRecord() };
    if (AudioManager::getInstance().prepareToRecord(recordingParams)) {
        AudioManager::getInstance().startRecording();
        mControlPanel->setRecordButtonState(RecordButton::State::recording);
        mPrepareToRecordWindow.reset();
    }
}

//==============================================================================
void MainContentComponent::resized()
{
    static constexpr auto MENU_BAR_HEIGHT = 20;

    JUCE_ASSERT_MESSAGE_THREAD;

    mMenuBar->setBounds(0, 0, getWidth(), MENU_BAR_HEIGHT);

    auto const availableBounds{ getLocalBounds().reduced(2).withTrimmedTop(MENU_BAR_HEIGHT) };

    // Lay out the speaker view and the vertical divider.
    Component * vComps[] = { mSpeakerViewComponent.get(), mVerticalDividerBar.get(), mMainLayout.get(), nullptr };

    // Lay out side-by-side and resize the components' heights as well as widths.
    mVerticalLayout.layOutComponents(vComps,
                                     3,
                                     availableBounds.getX(),
                                     availableBounds.getY(),
                                     availableBounds.getWidth(),
                                     availableBounds.getHeight(),
                                     false,
                                     true);
}
