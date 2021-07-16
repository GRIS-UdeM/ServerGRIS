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
#include "ControlPanel.hpp"
#include "FatalError.hpp"
#include "LegacyLbapPosition.hpp"
#include "MainWindow.hpp"
#include "ScopeGuard.hpp"
#include "TitledComponent.hpp"
#include "VuMeterComponent.hpp"
#include "constants.hpp"

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
static float gainToSourceAlpha(source_index_t const sourceIndex, float const gain)
{
    static constexpr auto RAMP = 0.12f;
    static constexpr auto OFF = 0.0f;
    static constexpr auto ON = 0.8f;
    static constexpr dbfs_t MIN_GAIN{ -80.0f };

    // TODO : static variables bad
    static StrongArray<source_index_t, float, MAX_NUM_SOURCES> lastAlphas{};
    auto & lastAlpha{ lastAlphas[sourceIndex] };

    if (dbfs_t::fromGain(gain) > MIN_GAIN) {
        lastAlpha = ON;
        return lastAlpha;
    }

    if (lastAlpha <= OFF) {
        return OFF;
    }

    lastAlpha = std::max(lastAlpha - RAMP, OFF);
    return lastAlpha;
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

        // Control panel
        mControlPanel = std::make_unique<ControlPanel>(*this, mLookAndFeel);
        mControlsSection = std::make_unique<TitledComponent>("Controls", mControlPanel.get(), mLookAndFeel);
        mControlPanel->setSpatMode(mData.speakerSetup.spatMode);
        mControlPanel->setCubeAttenuationDb(mData.project.lbapDistanceAttenuationData.attenuation);
        mControlPanel->setCubeAttenuationHz(mData.project.lbapDistanceAttenuationData.freq);
        mControlPanel->setStereoMode(mData.appData.stereoMode);
        mControlPanel->setStereoRouting(mData.appData.stereoRouting);

        // Source panel
        mSourcesInnerLayout
            = std::make_unique<LayoutComponent>(LayoutComponent::Orientation::horizontal, true, false, grisLookAndFeel);
        mSourcesOuterLayout = std::make_unique<LayoutComponent>(LayoutComponent::Orientation::horizontal,
                                                                false,
                                                                false,
                                                                grisLookAndFeel);
        mSourcesOuterLayout->addSection(*mSourcesInnerLayout).withRelativeSize(1.0f);
        mSourcesOuterLayout->addSection(mAddRemoveSourcesButton).withChildMinSize();
        mSourcesSection = std::make_unique<TitledComponent>("Sources", mSourcesOuterLayout.get(), grisLookAndFeel);

        // Speaker panel
        mSpeakersLayout
            = std::make_unique<LayoutComponent>(LayoutComponent::Orientation::horizontal, true, false, grisLookAndFeel);
        mSpeakersSection = std::make_unique<TitledComponent>("Speakers", mSpeakersLayout.get(), mLookAndFeel);

        // main sections
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
        mOscInput.reset(new OscInput(*this));
        mOscInput->startConnection(mData.project.oscPort);
    };

    auto const initAppData = [&]() { mData.appData = mConfiguration.load(); };
    auto const initProject = [&]() {
        auto const success{ loadProject(mData.appData.lastProject, true) };
        if (!success) {
            if (!loadProject(DEFAULT_PROJECT_FILE, true)) {
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
    initProject();
    initSpeakerSetup();
    initAudioManager();
    initAudioProcessor();

    // juce::ScopedLock const audioLock{ mAudioProcessor->getLock() };

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
    mOscInput.reset();

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
void MainContentComponent::handleNewProject()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const success{ loadProject(DEFAULT_PROJECT_FILE, false) };
    if (!success) {
        fatalError("Unable to load the default project file.", this);
    }
}

//==============================================================================
bool MainContentComponent::loadProject(juce::File const & file, bool const discardCurrentProject)
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
        auto const version{ SpatGrisVersion::fromString(
            mainXmlElem->getStringAttribute(SpatGrisProjectData::XmlTags::VERSION)) };
        if (version.compare(SPAT_GRIS_VERSION) > 0) {
            displayError("This project was created using a newer version of SpatGRIS that is not compatible with this "
                         "one.\nPlease upgrade to the latest version.");
            return false;
        }
        // Missing params
        displayError("File \"" + file.getFullPathName()
                     + "\" is missing one more mandatory parameters.\nYour file might be corrupted.");
        return false;
    }

    mData.project = std::move(*projectData);
    mData.appData.lastProject = file.getFullPathName();

    mControlPanel->setMasterGain(mData.project.masterGain);
    mControlPanel->setInterpolation(mData.project.spatGainsInterpolation);

    refreshAudioProcessor();
    refreshViewportConfig();
    setTitles();
    refreshSourceVuMeterComponents();

    return true;
}

//==============================================================================
void MainContentComponent::handleOpenProject()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    juce::File const & lastProject{ mData.appData.lastProject };
    juce::File const initialPath{ lastProject.isAChildOf(CURRENT_WORKING_DIR) ? juce::File::getSpecialLocation(
                                      juce::File::SpecialLocationType::userDocumentsDirectory)
                                                                              : lastProject };

    juce::FileChooser fc{ "Choose a file to open...", initialPath, "*.xml", true, false, this };

    if (!fc.browseForFileToOpen()) {
        return;
    }

    auto const chosen{ fc.getResult() };

    [[maybe_unused]] auto const success{ loadProject(chosen, false) };
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

    juce::File const lastSetup{ mData.appData.lastSpeakerSetup };

    auto const initialFile{ lastSetup.isAChildOf(CURRENT_WORKING_DIR) ? juce::File::getSpecialLocation(
                                juce::File::SpecialLocationType::userDocumentsDirectory)
                                                                      : lastSetup };

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

    if (mEditSpeakersWindow == nullptr) {
        auto const windowName = juce::String{ "Speakers Setup Edition - " }
                                + spatModeToString(mData.speakerSetup.spatMode) + " - "
                                + juce::File{ mData.appData.lastSpeakerSetup }.getFileNameWithoutExtension();
        mEditSpeakersWindow
            = std::make_unique<EditSpeakersWindow>(windowName, mLookAndFeel, *this, mData.appData.lastProject);
        mEditSpeakersWindow->initComp();
    }
}

//==============================================================================
void MainContentComponent::handleShowPreferences()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    if (mPropertiesWindow == nullptr) {
        mPropertiesWindow.reset(new SettingsWindow{ *this, mData.project.oscPort, mLookAndFeel });
        mPropertiesWindow->centreAroundComponent(this, mPropertiesWindow->getWidth(), mPropertiesWindow->getHeight());
    }
}

//==============================================================================
void MainContentComponent::handleShowOscMonitorWindow()
{
    if (!mOscMonitorWindow) {
        mOscMonitorWindow = std::make_unique<OscMonitorWindow>(*this, mLookAndFeel);
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

    refreshAudioProcessor();
}

//==============================================================================
void MainContentComponent::interpolationChanged(float const interpolation)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.spatGainsInterpolation = interpolation;
    mControlPanel->setInterpolation(interpolation);

    refreshAudioProcessor();
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
                speaker.position = speaker.position.normalized();
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

    mData.appData.viewSettings.showSpeakerTriplets = false;
    mData.appData.stereoMode = stereoMode;

    refreshSpatAlgorithm();
    refreshSpeakerVuMeterComponents();
    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::stereoRoutingChanged(StereoRouting const & routing)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.appData.stereoRouting = routing;
    refreshSpatAlgorithm();
}

//==============================================================================
void MainContentComponent::cubeAttenuationDbChanged(dbfs_t const value)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.lbapDistanceAttenuationData.attenuation = value;
    refreshAudioProcessor();
}

//==============================================================================
void MainContentComponent::cubeAttenuationHzChanged(hz_t const value)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.lbapDistanceAttenuationData.freq = value;
    refreshAudioProcessor();
}

//==============================================================================
void MainContentComponent::numSourcesChanged(int const numSources)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    jassert(numSources >= 1 && numSources <= MAX_NUM_SOURCES);
    juce::ScopedWriteLock const lock{ mLock };

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

    refreshAudioProcessor();
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
juce::Component * MainContentComponent::getControlsComponent() const
{
    return mMainLayout.get();
}

//==============================================================================
void MainContentComponent::handleShowNumbers()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSpeakerNumbers };
    var = !var;
    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSpeakers()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSpeakers };
    var = !var;
    refreshViewportConfig();
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
    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSourceLevel()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSourceActivity };
    var = !var;
    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSpeakerLevel()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSpeakerLevels };
    var = !var;
    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::handleShowSphere()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & var{ mData.appData.viewSettings.showSphereOrCube };
    var = !var;
    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::handleResetInputPositions()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    juce::ScopedWriteLock const lock{ mLock };

    auto & viewPortData{ mSpeakerViewComponent->getData() };
    auto & spatAlgorithm{ *mAudioProcessor->getSpatAlgorithm() };
    for (auto const source : mData.project.sources) {
        // reset positions
        source.value->position = tl::nullopt;

        {
            // reset 3d view
            auto & exchanger{ viewPortData.sourcesDataQueues[source.key] };
            auto * sourceTicket{ exchanger.acquire() };
            sourceTicket->get() = tl::nullopt;
            exchanger.setMostRecent(sourceTicket);
        }

        if (mFlatViewWindow) {
            // reset 2d view
            auto & exchanger{ mFlatViewWindow->getSourceDataQueues()[source.key] };
            auto * sourceTicket{ exchanger.acquire() };
            sourceTicket->get() = tl::nullopt;
            exchanger.setMostRecent(sourceTicket);
        }

        // spatData
        spatAlgorithm.updateSpatData(source.key, *source.value);
    }

    refreshAudioProcessor();
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

    const juce::CommandID ids[] = {
        MainWindow::CommandIds::newProjectId,
        MainWindow::CommandIds::openProjectId,
        MainWindow::CommandIds::saveProjectId,
        MainWindow::CommandIds::saveProjectAsId,
        MainWindow::CommandIds::openSpeakerSetupId,
        MainWindow::CommandIds::saveSpeakerSetupId,
        MainWindow::CommandIds::saveSpeakerSetupAsId,
        MainWindow::CommandIds::showSpeakerEditId,
        MainWindow::CommandIds::show2DViewId,
        MainWindow::CommandIds::showOscMonitorId,
        MainWindow::CommandIds::showNumbersId,
        MainWindow::CommandIds::showSpeakersId,
        MainWindow::CommandIds::showTripletsId,
        MainWindow::CommandIds::showSourceActivityId,
        MainWindow::CommandIds::showSpeakerActivityId,
        MainWindow::CommandIds::showSphereId,
        MainWindow::CommandIds::colorizeInputsId,
        MainWindow::CommandIds::resetInputPosId,
        MainWindow::CommandIds::resetMeterClipping,
        MainWindow::CommandIds::openSettingsWindowId,
        MainWindow::CommandIds::quitId,
        MainWindow::CommandIds::aboutId,
        MainWindow::CommandIds::openManualId,
    };

    commands.addArray(ids, juce::numElementsInArray(ids));

    auto const addTemplate = [&](auto const & templates) {
        for (auto const & speakerTemplate : templates) {
            commands.add(speakerTemplate.commandId);
        }
    };

    addTemplate(SPEAKER_SETUP_TEMPLATES.dome);
    addTemplate(SPEAKER_SETUP_TEMPLATES.cube);
    addTemplate(PROJECT_TEMPLATES);
}

//==============================================================================
void MainContentComponent::getCommandInfo(juce::CommandID const commandId, juce::ApplicationCommandInfo & result)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    const juce::String generalCategory("General");

    switch (commandId) {
    case MainWindow::newProjectId:
        result.setInfo("New Project", "Close the current project and open the default.", generalCategory, 0);
        result.addDefaultKeypress('N', juce::ModifierKeys::commandModifier);
        return;
    case MainWindow::openProjectId:
        result.setInfo("Open Project", "Choose a new project on disk.", generalCategory, 0);
        result.addDefaultKeypress('O', juce::ModifierKeys::commandModifier);
        return;
    case MainWindow::saveProjectId:
        result.setInfo("Save Project", "Save the current project on disk.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::commandModifier);
        result.setActive(isProjectModified());
        return;
    case MainWindow::saveProjectAsId:
        result.setInfo("Save Project As...", "Save the current project under a new name on disk.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::shiftModifier | juce::ModifierKeys::commandModifier);
        return;
    case MainWindow::openSpeakerSetupId:
        result.setInfo("Open Speaker Setup", "Choose a new speaker setup on disk.", generalCategory, 0);
        result.addDefaultKeypress('L', juce::ModifierKeys::commandModifier);
        return;
    case MainWindow::saveSpeakerSetupId:
        result.setInfo("Save Speaker Setup", "Save the current speaker setup on disk.", generalCategory, 0);
        result.addDefaultKeypress('E', juce::ModifierKeys::commandModifier);
        result.setActive(isSpeakerSetupModified());
        return;
    case MainWindow::saveSpeakerSetupAsId:
        result.setInfo("Save Speaker Setup As...",
                       "Save the current speaker setup under a new name on disk.",
                       generalCategory,
                       0);
        result.addDefaultKeypress('E', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
        return;
    case MainWindow::showSpeakerEditId:
        result.setInfo("Speaker Setup Edition", "Edit the current speaker setup.", generalCategory, 0);
        result.addDefaultKeypress('W', juce::ModifierKeys::altModifier);
        return;
    case MainWindow::show2DViewId:
        result.setInfo("Show 2D View", "Show the 2D action window.", generalCategory, 0);
        result.addDefaultKeypress('D', juce::ModifierKeys::altModifier);
        return;
    case MainWindow::showOscMonitorId:
        result.setInfo("Show OSC monitor", "Show the OSC monitor window", generalCategory, 0);
        return;
    case MainWindow::showNumbersId:
        result.setInfo("Show Numbers", "Show source and speaker numbers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('N', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSpeakerNumbers);
        return;
    case MainWindow::showSpeakersId:
        result.setInfo("Show Speakers", "Show speakers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('S', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSpeakers);
        return;
    case MainWindow::showTripletsId:
        result.setInfo("Show Speaker Triplets", "Show speaker triplets on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('T', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSpeakerTriplets);
        result.setActive(mAudioProcessor->getSpatAlgorithm()->hasTriplets());
        return;
    case MainWindow::showSourceActivityId:
        result.setInfo("Show Source Activity", "Activate brightness on sources on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('A', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSourceActivity);
        return;
    case MainWindow::showSpeakerActivityId:
        result.setInfo("Show Speaker Level", "Activate brightness on speakers on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('L', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSpeakerLevels);
        return;
    case MainWindow::showSphereId:
        result.setInfo("Show Sphere/Cube", "Show the sphere on the 3D view.", generalCategory, 0);
        result.addDefaultKeypress('O', juce::ModifierKeys::altModifier);
        result.setTicked(mData.appData.viewSettings.showSphereOrCube);
        return;
    case MainWindow::colorizeInputsId:
        result.setInfo("Colorize Inputs", "Spread the colour of the inputs over the colour range.", generalCategory, 0);
        result.addDefaultKeypress('C', juce::ModifierKeys::altModifier);
        return;
    case MainWindow::resetInputPosId:
        result.setInfo("Reset Input Position", "Reset the position of the input sources.", generalCategory, 0);
        result.addDefaultKeypress('R', juce::ModifierKeys::altModifier);
        return;
    case MainWindow::resetMeterClipping:
        result.setInfo("Reset Meter Clipping", "Reset clipping for all meters.", generalCategory, 0);
        result.addDefaultKeypress('M', juce::ModifierKeys::altModifier);
        return;
    case MainWindow::openSettingsWindowId:
        result.setInfo("Settings...", "Open the settings window.", generalCategory, 0);
        result.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
        return;
    case MainWindow::quitId:
        result.setInfo("Quit", "Quit the SpatGRIS.", generalCategory, 0);
        result.addDefaultKeypress('Q', juce::ModifierKeys::commandModifier);
        return;
    case MainWindow::aboutId:
        result.setInfo("About SpatGRIS", "Open the about window.", generalCategory, 0);
        return;
    case MainWindow::openManualId:
        result.setInfo("Open Documentation", "Open the manual in pdf viewer.", generalCategory, 0);
        return;
    }

    // probably a template
    auto const templateInfo{ commandIdToTemplate(commandId) };
    jassert(templateInfo);
    auto const longName{ juce::String{ "Open template " } + templateInfo->name };
    result.setInfo(longName, templateInfo->name, generalCategory, 0);
}

//==============================================================================
bool MainContentComponent::perform(InvocationInfo const & info)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (MainWindow::getMainAppWindow()) {
        switch (info.commandID) {
        case MainWindow::newProjectId:
            handleNewProject();
            break;
        case MainWindow::openProjectId:
            handleOpenProject();
            break;
        case MainWindow::saveProjectId:
            handleSaveProject();
            break;
        case MainWindow::saveProjectAsId:
            handleSaveProjectAs();
            break;
        case MainWindow::openSpeakerSetupId:
            handleOpenSpeakerSetup();
            break;
        case MainWindow::saveSpeakerSetupId:
            handleSaveSpeakerSetup();
            break;
        case MainWindow::saveSpeakerSetupAsId:
            handleSaveSpeakerSetupAs();
            break;
        case MainWindow::showSpeakerEditId:
            handleShowSpeakerEditWindow();
            break;
        case MainWindow::show2DViewId:
            handleShow2DView();
            break;
        case MainWindow::showOscMonitorId:
            handleShowOscMonitorWindow();
            break;
        case MainWindow::showNumbersId:
            handleShowNumbers();
            break;
        case MainWindow::showSpeakersId:
            handleShowSpeakers();
            break;
        case MainWindow::showTripletsId:
            handleShowTriplets();
            break;
        case MainWindow::showSourceActivityId:
            handleShowSourceLevel();
            break;
        case MainWindow::showSpeakerActivityId:
            handleShowSpeakerLevel();
            break;
        case MainWindow::showSphereId:
            handleShowSphere();
            break;
        case MainWindow::colorizeInputsId:
            handleColorizeInputs();
            break;
        case MainWindow::resetInputPosId:
            handleResetInputPositions();
            break;
        case MainWindow::resetMeterClipping:
            handleResetMeterClipping();
            break;
        case MainWindow::openSettingsWindowId:
            handleShowPreferences();
            break;
        case MainWindow::quitId:
            dynamic_cast<MainWindow *>(&mMainWindow)->closeButtonPressed();
            break;
        case MainWindow::aboutId:
            handleShowAbout();
            break;
        case MainWindow::openManualId:
            handleOpenManual();
            break;
        default:
            // open a template
            auto const templateInfo{ commandIdToTemplate(info.commandID) };
            if (!templateInfo) {
                return false;
            }
            if (templateInfo->path.isAChildOf(SPEAKER_TEMPLATES_DIR)) {
                loadSpeakerSetup(templateInfo->path, LoadSpeakerSetupOption::disallowDiscardingUnsavedChanges);
                break;
            }
            jassert(templateInfo->path.isAChildOf(PROJECT_TEMPLATES_DIR));
            loadProject(templateInfo->path, false);
            break;
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

    if (currentAudioDevice) {
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

    refreshSpeakers();
}

//==============================================================================
juce::PopupMenu MainContentComponent::getMenuForIndex(int /*menuIndex*/, const juce::String & menuName)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    juce::ApplicationCommandManager * commandManager = &mMainWindow.getApplicationCommandManager();

    auto const extractTemplatesToMenu = [&](auto const & templates) {
        juce::PopupMenu menu{};
        for (auto const & setupTemplate : templates) {
            menu.addCommandItem(commandManager, setupTemplate.commandId, setupTemplate.name);
        }
        return menu;
    };

    auto const getSpeakerSetupTemplatesMenu = [&]() {
        juce::PopupMenu menu{};
        menu.addSubMenu("Dome", extractTemplatesToMenu(SPEAKER_SETUP_TEMPLATES.dome));
        menu.addSubMenu("Cube", extractTemplatesToMenu(SPEAKER_SETUP_TEMPLATES.cube));
        return menu;
    };

    juce::PopupMenu menu;

    if (menuName == "File") {
        menu.addCommandItem(commandManager, MainWindow::newProjectId);
        menu.addCommandItem(commandManager, MainWindow::openProjectId);
        menu.addSubMenu("Templates", extractTemplatesToMenu(PROJECT_TEMPLATES));
        menu.addCommandItem(commandManager, MainWindow::saveProjectId);
        menu.addCommandItem(commandManager, MainWindow::saveProjectAsId);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::openSpeakerSetupId);
        menu.addSubMenu("Templates", getSpeakerSetupTemplatesMenu());
        menu.addCommandItem(commandManager, MainWindow::saveSpeakerSetupId);
        menu.addCommandItem(commandManager, MainWindow::saveSpeakerSetupAsId);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::openSettingsWindowId);
#if !JUCE_MAC
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::quitId);
#endif
    } else if (menuName == "View") {
        menu.addCommandItem(commandManager, MainWindow::show2DViewId);
        menu.addCommandItem(commandManager, MainWindow::showSpeakerEditId);
        menu.addCommandItem(commandManager, MainWindow::showOscMonitorId);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::showNumbersId);
        menu.addCommandItem(commandManager, MainWindow::showSpeakersId);
        if (mAudioProcessor->getSpatAlgorithm()->hasTriplets()) {
            menu.addCommandItem(commandManager, MainWindow::showTripletsId);
        } else {
            menu.addItem(MainWindow::showTripletsId, "Show Speaker Triplets", false, false);
        }
        menu.addCommandItem(commandManager, MainWindow::showSourceActivityId);
        menu.addCommandItem(commandManager, MainWindow::showSpeakerActivityId);
        menu.addCommandItem(commandManager, MainWindow::showSphereId);
        menu.addSeparator();
        menu.addCommandItem(commandManager, MainWindow::colorizeInputsId);
        menu.addCommandItem(commandManager, MainWindow::resetInputPosId);
        menu.addCommandItem(commandManager, MainWindow::resetMeterClipping);
    } else if (menuName == "Help") {
        menu.addCommandItem(commandManager, MainWindow::aboutId);
        menu.addCommandItem(commandManager, MainWindow::openManualId);
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

    auto & viewPortData{ mSpeakerViewComponent->getData() };
    auto * flatViewViewportDataQueues{ mFlatViewWindow ? &mFlatViewWindow->getSourceDataQueues() : nullptr };

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

        auto const data{ sourceData.value->toViewportData(
            mData.appData.viewSettings.showSourceActivity ? gainToSourceAlpha(sourceData.key, peak) : 0.8f) };

        // update 3d view
        if (!viewPortData.sourcesDataQueues.contains(sourceData.key)) {
            viewPortData.sourcesDataQueues.add(sourceData.key);
        }

        {
            auto & exchanger{ viewPortData.sourcesDataQueues[sourceData.key] };
            auto * ticket{ exchanger.acquire() };
            ticket->get() = data;
            exchanger.setMostRecent(ticket);
        }

        // update 2d view
        if (flatViewViewportDataQueues) {
            auto & exchanger{ (*flatViewViewportDataQueues)[sourceData.key] };
            auto * ticket{ exchanger.acquire() };
            ticket->get() = data;
            exchanger.setMostRecent(ticket);
        }
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

        auto & exchanger{ viewPortData.speakersAlphaQueues[speaker.key] };
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

    mSourcesInnerLayout->clearSections();
    mSourceVuMeterComponents.clear();

    auto const isAtLeastOneSourceSolo{ std::any_of(
        mData.project.sources.cbegin(),
        mData.project.sources.cend(),
        [](SourcesData::ConstNode const & node) { return node.value->state == PortState::solo; }) };

    // auto x{ 3 };
    for (auto source : mData.project.sources) {
        auto newVuMeter{ std::make_unique<SourceVuMeterComponent>(source.key,
                                                                  source.value->directOut,
                                                                  source.value->colour,
                                                                  *this,
                                                                  mSmallLookAndFeel) };
        auto const & state{ source.value->state };
        newVuMeter->setState(state, isAtLeastOneSourceSolo);
        auto & addedVuMeter{ mSourceVuMeterComponents.add(source.key, std::move(newVuMeter)) };
        mSourcesInnerLayout->addSection(&addedVuMeter).withChildMinSize();
    }

    mSourcesInnerLayout->resized();
}

//==============================================================================
void MainContentComponent::refreshSpeakerVuMeterComponents()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    mSpeakersLayout->clearSections();
    mSpeakerVuMeterComponents.clear();
    mStereoVuMeterComponents.clearQuick(true);

    auto const isAtLeastOneSpeakerSolo{ std::any_of(
        mData.speakerSetup.speakers.cbegin(),
        mData.speakerSetup.speakers.cend(),
        [](SpeakersData::ConstNode const & node) { return node.value->state == PortState::solo; }) };

    if (mData.appData.stereoMode) {
        mSpeakersLayout
            ->addSection(mStereoVuMeterComponents.add(std::make_unique<StereoVuMeterComponent>("L", mSmallLookAndFeel)))
            .withChildMinSize();
        mSpeakersLayout
            ->addSection(mStereoVuMeterComponents.add(std::make_unique<StereoVuMeterComponent>("R", mSmallLookAndFeel)))
            .withChildMinSize();
    }

    for (auto const outputPatch : mData.speakerSetup.ordering) {
        auto newVuMeter{ std::make_unique<SpeakerVuMeterComponent>(outputPatch, *this, mSmallLookAndFeel) };
        auto const & state{ mData.speakerSetup.speakers[outputPatch].state };
        newVuMeter->setState(state, isAtLeastOneSpeakerSolo);
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
void MainContentComponent::setSourcePosition(source_index_t const sourceIndex,
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

    auto const getCorrectedPosition = [&]() -> Position {
        switch (mData.speakerSetup.spatMode) {
        case SpatMode::vbap: {
            return Position{ PolarVector{ azimuth, elevation, length }.normalized() };
        }
        case SpatMode::lbap: {
            return LegacyLbapPosition{ azimuth, elevation, length }.toPosition();
        }
        }
        jassertfalse;
        return {};
    };

    auto const correctedPosition{ getCorrectedPosition() };
    auto & source{ mData.project.sources[sourceIndex] };

    if (correctedPosition == source.position && newAzimuthSpan == source.azimuthSpan
        && newZenithSpan == source.zenithSpan) {
        return;
    }

    juce::ScopedWriteLock const writeLock{ mLock };

    source.position = correctedPosition;
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
}

//==============================================================================
void MainContentComponent::speakerOnlyDirectOutChanged(output_patch_t const outputPatch, bool const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & speaker{ mData.speakerSetup.speakers[outputPatch] };
    auto & val{ speaker.isDirectOutOnly };
    if (state != val) {
        val = state;

        if (!state && mData.speakerSetup.spatMode == SpatMode::vbap) {
            speaker.position = speaker.position.normalized();
        }

        refreshAudioProcessor();
        refreshViewportConfig();
    }
}

//==============================================================================
void MainContentComponent::speakerOutputPatchChanged(output_patch_t const oldOutputPatch,
                                                     output_patch_t const newOutputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & speakers{ mData.speakerSetup.speakers };
    speakers.add(newOutputPatch, std::make_unique<SpeakerData>(speakers[oldOutputPatch]));
    speakers.remove(oldOutputPatch);

    auto & order{ mData.speakerSetup.ordering };
    order.set(order.indexOf(oldOutputPatch), newOutputPatch);

    refreshSpeakerVuMeterComponents();
}

//==============================================================================
void MainContentComponent::setSpeakerGain(output_patch_t const outputPatch, dbfs_t const gain)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.speakerSetup.speakers[outputPatch].gain = gain;

    refreshAudioProcessor();
}

//==============================================================================
void MainContentComponent::setSpeakerHighPassFreq(output_patch_t const outputPatch, hz_t const freq)
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
void MainContentComponent::setPinkNoiseGain(tl::optional<dbfs_t> const gain)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (gain != mData.pinkNoiseLevel) {
        mData.pinkNoiseLevel = gain;
        mAudioProcessor->setAudioConfig(mData.toAudioConfig());
    }
}

//==============================================================================
void MainContentComponent::setSourceColor(source_index_t const sourceIndex, juce::Colour const colour)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (sourceIndex.get() == mData.project.sources.size() + 1) {
        // Right click on the last source's color selector
        return;
    }

    mData.project.sources[sourceIndex].colour = colour;
    mSourceVuMeterComponents[sourceIndex].setSourceColour(colour);
}

//==============================================================================
void MainContentComponent::setSourceState(source_index_t const sourceIndex, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.sources[sourceIndex].state = state;

    refreshAudioProcessor();
    refreshSourceVuMeterComponents();
}

//==============================================================================
void MainContentComponent::setSelectedSpeakers(juce::Array<output_patch_t> const selection)
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

    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::setSpeakerState(output_patch_t const outputPatch, PortState const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.speakerSetup.speakers[outputPatch].state = state;

    refreshAudioProcessor();
    refreshSpeakerVuMeterComponents();
}

//==============================================================================
void MainContentComponent::setSourceDirectOut(source_index_t const sourceIndex,
                                              tl::optional<output_patch_t> const outputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.project.sources[sourceIndex].directOut = outputPatch;

    if (mData.appData.stereoMode) {
        refreshSpeakers();
    } else {
        refreshAudioProcessor();
    }

    mSourceVuMeterComponents[sourceIndex].setDirectOut(outputPatch);
}

//==============================================================================
void MainContentComponent::refreshAudioProcessor() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    if (!mAudioProcessor) {
        return;
    }

    mAudioProcessor->setAudioConfig(mData.toAudioConfig());
}

//==============================================================================
void MainContentComponent::refreshSpatAlgorithm()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    if (!mAudioProcessor) {
        return;
    }

    juce::ScopedLock const audioLock{ mAudioProcessor->getLock() };

    auto & oldSpatAlgorithm{ mAudioProcessor->getSpatAlgorithm() };
    auto newSpatAlgorithm{ AbstractSpatAlgorithm::make(mData.speakerSetup,
                                                       mData.appData.stereoMode,
                                                       mData.project.sources,
                                                       mData.appData.audioSettings.sampleRate,
                                                       mData.appData.audioSettings.bufferSize) };

    if (newSpatAlgorithm->getError()
        && (!oldSpatAlgorithm || oldSpatAlgorithm->getError() != newSpatAlgorithm->getError())) {
        switch (*newSpatAlgorithm->getError()) {
        case AbstractSpatAlgorithm::Error::notEnoughDomeSpeakers:
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::AlertIconType::InfoIcon,
                                                   "Disabled spatialization",
                                                   "Domes need at least 3 speakers.\n",
                                                   "Ok",
                                                   this);
            break;
        case AbstractSpatAlgorithm::Error::notEnoughCubeSpeakers:
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::AlertIconType::InfoIcon,
                                                   "Disabled spatialization",
                                                   "The Cubes need at least 2 speakers.\n",
                                                   "Ok",
                                                   this);
            break;
        case AbstractSpatAlgorithm::Error::flatDomeSpeakersTooFarApart:
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::AlertIconType::InfoIcon,
                                                   "Disabled spatialization",
                                                   "If all speakers are at the same height, Domes require their "
                                                   "speakers not to be more than 170 degrees apart from each others.\n",
                                                   "Ok",
                                                   this);
            break;
        default:
            jassertfalse;
            break;
        }
    }

    oldSpatAlgorithm = std::move(newSpatAlgorithm);

    refreshAudioProcessor();
    reassignSourcesPositions();
}

//==============================================================================
void MainContentComponent::refreshViewportConfig() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    if (!mAudioProcessor) {
        return;
    }

    auto const & spatAlgorithm{ *mAudioProcessor->getSpatAlgorithm() };

    auto const newConfig{ mData.toViewportConfig() };
    if (newConfig.viewSettings.showSpeakerTriplets && spatAlgorithm.hasTriplets()) {
        mSpeakerViewComponent->setTriplets(spatAlgorithm.getTriplets());
    }
    mSpeakerViewComponent->setConfig(newConfig, mData.project.sources);
}

//==============================================================================
void MainContentComponent::setShowTriplets(bool const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    mData.appData.viewSettings.showSpeakerTriplets = state;
    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::reorderSpeakers(juce::Array<output_patch_t> newOrder)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    auto & order{ mData.speakerSetup.ordering };
    jassert(newOrder.size() == order.size());
    order = std::move(newOrder);
    refreshSpeakerVuMeterComponents();
}

//==============================================================================
output_patch_t MainContentComponent::getMaxSpeakerOutputPatch() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    auto const & patches{ mData.speakerSetup.ordering };
    auto const * maxIterator{ std::max_element(patches.begin(), patches.end()) };
    auto const maxPatch{ maxIterator != mData.speakerSetup.ordering.end() ? *maxIterator : output_patch_t{} };
    return maxPatch;
}

//==============================================================================
tl::optional<SpeakerSetup> MainContentComponent::extractSpeakerSetup(juce::File const & file)
{
    auto const displayError = [&](juce::String const & message) {
        juce::AlertWindow::showNativeDialogBox("Unable to open Speaker Setup.", message, false);
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
        auto const version{ SpatGrisVersion::fromString(
            mainXmlElem->getStringAttribute(SpeakerSetup::XmlTags::VERSION)) };
        if (version.compare(SPAT_GRIS_VERSION) > 0) {
            displayError("This speaker setup was created using a newer version of SpatGRIS that is not compatible with "
                         "this one.\nPlease upgrade to the latest version.");
        } else {
            displayError("File \"" + file.getFullPathName()
                         + "\" is missing one more mandatory parameters.\nYour file might be corrupted.");
        }
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
        auto const isValidIndex{ *index >= 0 && *index < mData.speakerSetup.ordering.size() };
        if (isValidIndex) {
            mData.speakerSetup.ordering.insert(*index, newOutputPatch);
        } else {
            static constexpr auto AT_END = -1;
            mData.speakerSetup.ordering.insert(AT_END, newOutputPatch);
        }
    } else {
        mData.speakerSetup.ordering.add(newOutputPatch);
    }

    mData.speakerSetup.speakers.add(newOutputPatch, std::move(newSpeaker));

    return newOutputPatch;
}

//==============================================================================
void MainContentComponent::removeSpeaker(output_patch_t const outputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const dataLock{ mLock };
    juce::ScopedLock const audioLock{ mAudioProcessor->getLock() };

    mData.speakerSetup.ordering.removeFirstMatchingValue(outputPatch);
    mData.speakerSetup.speakers.remove(outputPatch);

    [[maybe_unused]] auto const success{ refreshSpeakers() };
}

//==============================================================================
bool MainContentComponent::refreshSpeakers()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    if (!mAudioProcessor) {
        return false;
    }

    refreshSpatAlgorithm();

    if (!mAudioProcessor->getSpatAlgorithm()->hasTriplets()) {
        mData.appData.viewSettings.showSpeakerTriplets = false;
    }

    refreshSpeakerVuMeterComponents();
    refreshViewportConfig();
    if (mEditSpeakersWindow != nullptr) {
        mEditSpeakersWindow->updateWinContent();
    }
    mControlPanel->updateSpeakers(mData.speakerSetup.ordering, mData.appData.stereoRouting);

    return true;
}

//==============================================================================
bool MainContentComponent::loadSpeakerSetup(juce::File const & file, LoadSpeakerSetupOption const option)
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

    setTitles();

    return true;
}

//==============================================================================
void MainContentComponent::setTitles() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    auto const projectFileName{ juce::File{ mData.appData.lastProject }.getFileNameWithoutExtension() };
    auto const mainWindowTitle{ juce::String{ "SpatGRIS v" }
                                + juce::JUCEApplication::getInstance()->getApplicationVersion() + " - "
                                + projectFileName };
    mMainWindow.DocumentWindow::setName(mainWindowTitle);

    if (mEditSpeakersWindow != nullptr) {
        auto const speakerSetupFileName{ juce::File{ mData.appData.lastSpeakerSetup }.getFileNameWithoutExtension() };
        auto const windowName{ juce::String{ "Speakers Setup Edition - " }
                               + spatModeToString(mData.speakerSetup.spatMode) + juce::String(" - ")
                               + speakerSetupFileName };
        mEditSpeakersWindow->setName(windowName);
    }

    refreshViewportConfig();
}

//==============================================================================
void MainContentComponent::reassignSourcesPositions()
{
    for (auto const & source : mData.project.sources) {
        if (!source.value->position) {
            continue;
        }
        updateSourceSpatData(source.key);
    }
}

//==============================================================================
void MainContentComponent::buttonPressed([[maybe_unused]] SpatButton * button)
{
    jassert(button == &mAddRemoveSourcesButton);
    mAddRemoveSourcesWindow
        = std::make_unique<AddRemoveSourcesWindow>(mData.project.sources.size(), *this, mLookAndFeel);
}

//==============================================================================
void MainContentComponent::handleSaveSpeakerSetup()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedReadLock const lock{ mLock };

    saveSpeakerSetup(mData.appData.lastSpeakerSetup);
}

//==============================================================================
bool MainContentComponent::saveProject(tl::optional<juce::File> maybeFile)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    static auto const IS_SAVABLE = [](juce::File const & file) { return !file.isAChildOf(CURRENT_WORKING_DIR); };

    if (!maybeFile || !IS_SAVABLE(*maybeFile)) {
        juce::File const lastProjectFile{ mData.appData.lastProject };
        auto const initialFile{ lastProjectFile.isAChildOf(CURRENT_WORKING_DIR)
                                    ? juce::File::getSpecialLocation(
                                          juce::File::SpecialLocationType::userDocumentsDirectory)
                                          .getChildFile(lastProjectFile.getFileName())
                                    : lastProjectFile };
        juce::FileChooser fc{ "Choose file to save to...", initialFile, "*.xml", true, false, this };
        if (!fc.browseForFileToSave(true)) {
            return false;
        }

        maybeFile = fc.getResult();
    }

    auto const file{ *maybeFile };
    auto const content{ mData.project.toXml() };
    auto const success{ content->writeTo(file) };
    jassert(success);

    if (success) {
        mData.appData.lastProject = maybeFile->getFullPathName();
    }

    setTitles();

    return success;
}

//==============================================================================
bool MainContentComponent::saveSpeakerSetup(tl::optional<juce::File> maybeFile)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    juce::ScopedWriteLock const lock{ mLock };

    static auto const IS_SAVABLE = [](juce::File const & file) { return !file.isAChildOf(CURRENT_WORKING_DIR); };

    if (!maybeFile || !IS_SAVABLE(*maybeFile)) {
        juce::File const & lastSpeakerSetup{ mData.appData.lastSpeakerSetup };
        auto const initialFile{ lastSpeakerSetup.isAChildOf(CURRENT_WORKING_DIR)
                                    ? juce::File::getSpecialLocation(
                                          juce::File::SpecialLocationType::userDocumentsDirectory)
                                          .getChildFile(juce::File{ mData.appData.lastSpeakerSetup }.getFileName())
                                    : lastSpeakerSetup };
        juce::FileChooser fc{ "Choose file to save to...", initialFile, "*.xml", true, false, this };
        if (!fc.browseForFileToSave(true)) {
            return false;
        }

        maybeFile = fc.getResult();
    }

    auto const & file{ *maybeFile };
    auto const content{ mData.speakerSetup.toXml() };
    auto const success{ content->writeTo(file) };

    jassert(success);
    if (success) {
        mData.appData.lastSpeakerSetup = maybeFile->getFullPathName();
    }

    setTitles();

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
    auto const getSpeakersToRecord = [&]() -> juce::Array<output_patch_t> {
        juce::Array<output_patch_t> result{};

        if (mData.appData.stereoMode) {
            auto const & routing{ mData.appData.stereoRouting };
            result.add(routing.left);
            result.add(routing.right);
            return result;
        }

        result = mData.speakerSetup.ordering;
        result.sort();

        return result;
    };

    auto speakersToRecord{ getSpeakersToRecord() };

    for (auto const & outputPatch : speakersToRecord) {
        if (!mData.speakerSetup.ordering.contains(outputPatch)) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::AlertIconType::WarningIcon,
                "Recording canceled",
                "Recording cannot start because an output patch used in either in your direct outputs or in your "
                "stereo reduction channels does not exists in the current speaker setup.",
                "Ok",
                this);
            return;
        }
    }

    AudioManager::RecordingParameters const recordingParams{ fileOrDirectory.getFullPathName(),
                                                             mData.appData.recordingOptions,
                                                             mData.appData.audioSettings.sampleRate,
                                                             std::move(speakersToRecord) };
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
