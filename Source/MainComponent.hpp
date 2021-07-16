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

#include "AboutWindow.hpp"
#include "AddRemoveSourcesWindow.hpp"
#include "AudioProcessor.hpp"
#include "Configuration.hpp"
#include "ControlPanel.hpp"
#include "EditSpeakersWindow.hpp"
#include "FlatViewWindow.hpp"
#include "InfoPanel.hpp"
#include "LayoutComponent.hpp"
#include "LogicStrucs.hpp"
#include "OscInput.hpp"
#include "OscMonitor.hpp"
#include "OwnedMap.hpp"
#include "PrepareToRecordWindow.hpp"
#include "SettingsWindow.hpp"
#include "SpatButton.hpp"
#include "SpeakerViewComponent.hpp"
#include "StrongTypes.hpp"
#include "TitledComponent.hpp"
#include "Triplet.hpp"
#include "VuMeterComponent.hpp"
#include "constants.hpp"

class MainWindow;

//==============================================================================
class AudioDeviceManagerListener : public juce::ChangeListener
{
protected:
    virtual void audioParametersChanged() = 0;

private:
    void changeListenerCallback([[maybe_unused]] juce::ChangeBroadcaster * source) override
    {
        jassert(dynamic_cast<juce::AudioDeviceManager *>(source));
        audioParametersChanged();
    }
};

//==============================================================================
class MainContentComponent final
    : public juce::Component
    , public juce::MenuBarModel
    , public juce::ApplicationCommandTarget
    , public SourceVuMeterComponent::Owner
    , public SpeakerVuMeterComponent::Owner
    , public ControlPanel::Listener
    , private SpatButton::Listener
    , private AudioDeviceManagerListener
    , private juce::Timer
{
    enum class LoadSpeakerSetupOption { allowDiscardingUnsavedChanges, disallowDiscardingUnsavedChanges };

    juce::ReadWriteLock mLock{};

    std::unique_ptr<AudioProcessor> mAudioProcessor{};

    OwnedMap<source_index_t, SourceVuMeterComponent, MAX_NUM_SOURCES> mSourceVuMeterComponents{};
    OwnedMap<output_patch_t, SpeakerVuMeterComponent, MAX_NUM_SPEAKERS> mSpeakerVuMeterComponents{};
    juce::OwnedArray<StereoVuMeterComponent> mStereoVuMeterComponents{};

    std::unique_ptr<OscInput> mOscInput{};

    // Windows.
    std::unique_ptr<EditSpeakersWindow> mEditSpeakersWindow{};
    std::unique_ptr<SettingsWindow> mPropertiesWindow{};
    std::unique_ptr<FlatViewWindow> mFlatViewWindow{};
    std::unique_ptr<AboutWindow> mAboutWindow{};
    std::unique_ptr<PrepareToRecordWindow> mPrepareToRecordWindow{};
    std::unique_ptr<OscMonitorWindow> mOscMonitorWindow{};
    std::unique_ptr<AddRemoveSourcesWindow> mAddRemoveSourcesWindow{};

    //==============================================================================
    // info section
    std::unique_ptr<InfoPanel> mInfoPanel{};

    // Sources Section
    SpatButton mAddRemoveSourcesButton{ "+/-", "Add or remove sources.", 30, 0, *this };
    std::unique_ptr<LayoutComponent> mSourcesInnerLayout{};
    std::unique_ptr<LayoutComponent> mSourcesOuterLayout{};
    std::unique_ptr<TitledComponent> mSourcesSection{};

    // Speakers Section
    std::unique_ptr<TitledComponent> mSpeakersSection{};
    std::unique_ptr<LayoutComponent> mSpeakersLayout{};

    // Controls section
    std::unique_ptr<TitledComponent> mControlsSection{};
    std::unique_ptr<ControlPanel> mControlPanel{};

    std::unique_ptr<LayoutComponent> mMainLayout{};
    //==============================================================================
    std::unique_ptr<SpeakerViewComponent> mSpeakerViewComponent{};
    juce::StretchableLayoutManager mVerticalLayout{};
    std::unique_ptr<juce::StretchableLayoutResizerBar> mVerticalDividerBar{};

    std::unique_ptr<juce::SplashScreen> mSplashScreen{};

    bool mIsProcessForeground{ true };

    GrisLookAndFeel & mLookAndFeel;
    SmallGrisLookAndFeel & mSmallLookAndFeel;

    MainWindow & mMainWindow;

    std::unique_ptr<juce::MenuBarComponent> mMenuBar{};
    //==============================================================================
    // App user settings.

    Configuration mConfiguration;
    juce::Rectangle<int> mFlatViewWindowRect{};

    //==============================================================================
    // State
    SpatGrisData mData{};

public:
    //==============================================================================
    MainContentComponent(MainWindow & mainWindow,
                         GrisLookAndFeel & grisLookAndFeel,
                         SmallGrisLookAndFeel & smallGrisLookAndFeel);
    //==============================================================================
    MainContentComponent() = delete;
    ~MainContentComponent() override;

    MainContentComponent(MainContentComponent const &) = delete;
    MainContentComponent(MainContentComponent &&) = delete;

    MainContentComponent & operator=(MainContentComponent const &) = delete;
    MainContentComponent & operator=(MainContentComponent &&) = delete;
    //==============================================================================
    [[nodiscard]] bool exitApp();

    auto const & getData() const noexcept { return mData; }
    auto const & getLock() const { return mLock; }
    [[nodiscard]] SpeakersData const & getSpeakersData() const override { return mData.speakerSetup.speakers; }

    void setSourcePosition(source_index_t sourceIndex,
                           radians_t azimuth,
                           radians_t elevation,
                           float length,
                           float newAzimuthSpan,
                           float newZenithSpan);

    void resetSourcePosition(source_index_t sourceIndex);

    void speakerOnlyDirectOutChanged(output_patch_t outputPatch, bool state);
    void speakerOutputPatchChanged(output_patch_t oldOutputPatch, output_patch_t newOutputPatch);
    void setSpeakerGain(output_patch_t outputPatch, dbfs_t gain);
    void setSpeakerHighPassFreq(output_patch_t outputPatch, hz_t freq);

    void setPinkNoiseGain(tl::optional<dbfs_t> gain);

    void setSourceColor(source_index_t sourceIndex, juce::Colour colour) override;
    void setSourceState(source_index_t sourceIndex, PortState state) override;
    void setSelectedSpeakers(juce::Array<output_patch_t> selection) override;
    void setSpeakerState(output_patch_t outputPatch, PortState state) override;
    void setSourceDirectOut(source_index_t sourceIndex, tl::optional<output_patch_t> outputPatch) override;
    void setShowTriplets(bool state);

    template<typename T>
    void setSpeakerPosition(output_patch_t const outputPatch, T const & position)
    {
        JUCE_ASSERT_MESSAGE_THREAD;
        juce::ScopedWriteLock const lock{ mLock };

        auto & speaker{ mData.speakerSetup.speakers[outputPatch] };
        speaker.position = position;

        refreshViewportConfig();
    }

    void refreshViewportConfig() const; // TODO: should be private

    //==============================================================================
    // Control Panel
    [[nodiscard]] bool setSpatMode(SpatMode spatMode);
    void setStereoMode(tl::optional<StereoMode> stereoMode);
    void setStereoRouting(StereoRouting const & routing);
    void cubeAttenuationDbChanged(dbfs_t value);
    void cubeAttenuationHzChanged(hz_t value);
    void numSourcesChanged(int numSources);
    void masterGainChanged(dbfs_t gain);
    void interpolationChanged(float interpolation);
    void recordButtonPressed() override;

    juce::Component * getControlsComponent() const;

    output_patch_t addSpeaker(tl::optional<output_patch_t> speakerToCopy, tl::optional<int> index);
    void removeSpeaker(output_patch_t outputPatch);
    void reorderSpeakers(juce::Array<output_patch_t> newOrder);

    [[nodiscard]] AudioProcessor & getAudioProcessor() { return *mAudioProcessor; }
    [[nodiscard]] AudioProcessor const & getAudioProcessor() const { return *mAudioProcessor; }

    [[nodiscard]] std::unique_ptr<OscMonitorWindow> & getOscMonitor() { return mOscMonitorWindow; }

    [[nodiscard]] bool refreshSpeakers();

    //==============================================================================
    // Commands.
    void handleShowPreferences();

    //==============================================================================
    // Close windows
    void closeSpeakersConfigurationWindow();
    void closePropertiesWindow() { mPropertiesWindow.reset(); }
    void closeFlatViewWindow() { mFlatViewWindow.reset(); }
    void closeAboutWindow() { mAboutWindow.reset(); }
    void closeOscMonitorWindow() { mOscMonitorWindow.reset(); }
    void closePrepareToRecordWindow() { mPrepareToRecordWindow.reset(); }
    void closeAddRemoveSourcesWindow() { mAddRemoveSourcesWindow.reset(); }
    //==============================================================================
    void prepareAndStartRecording(juce::File const & fileOrDirectory, RecordingOptions const & recordingOptions);

private:
    //==============================================================================
    [[nodiscard]] bool isProjectModified() const;
    [[nodiscard]] bool isSpeakerSetupModified() const;

    //==============================================================================
    // MenuBar handlers.
    void handleNewProject();
    void handleOpenProject();
    void handleSaveProject();
    void handleSaveProjectAs();
    void handleOpenSpeakerSetup();
    void handleShowSpeakerEditWindow();
    void handleShowAbout();
    void handleShow2DView();
    void handleShowNumbers();
    void handleShowSpeakers();
    void handleShowTriplets();
    void handleShowSourceLevel();
    void handleShowSpeakerLevel();
    void handleShowSphere();
    void handleResetInputPositions();
    void handleResetMeterClipping();
    void handleColorizeInputs();
    void handleSaveSpeakerSetup();
    void handleSaveSpeakerSetupAs();
    void handleShowOscMonitorWindow();

    void refreshSourceVuMeterComponents();
    void refreshSpeakerVuMeterComponents();

    void updateSourceSpatData(source_index_t sourceIndex);

    void refreshAudioProcessor() const;
    void refreshSpatAlgorithm();
    void updatePeaks();
    [[nodiscard]] output_patch_t getMaxSpeakerOutputPatch() const;
    void reassignSourcesPositions();
    //==============================================================================
    // Open - save.
    [[nodiscard]] static tl::optional<SpeakerSetup> extractSpeakerSetup(juce::File const & file);
    bool loadSpeakerSetup(juce::File const & file, LoadSpeakerSetupOption option);
    bool loadProject(juce::File const & file, bool discardCurrentProject);
    bool saveProject(tl::optional<juce::File> maybeFile);
    bool saveSpeakerSetup(tl::optional<juce::File> maybeFile);
    [[nodiscard]] bool makeSureProjectIsSavedToDisk() noexcept;
    [[nodiscard]] bool makeSureSpeakerSetupIsSavedToDisk() noexcept;
    void setTitles() const;

    void timerCallback() override;
    void paint(juce::Graphics & g) override;
    void resized() override;
    void menuItemSelected(int menuItemId, int /*topLevelMenuIndex*/) override;
    [[nodiscard]] juce::StringArray getMenuBarNames() override;
    [[nodiscard]] juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String & /*menuName*/) override;

    void buttonPressed(SpatButton * button) override;
    void audioParametersChanged() override;

    void getAllCommands(juce::Array<juce::CommandID> & commands) override;
    void getCommandInfo(juce::CommandID commandId, juce::ApplicationCommandInfo & result) override;
    [[nodiscard]] juce::ApplicationCommandTarget * getNextCommandTarget() override;
    [[nodiscard]] bool perform(juce::ApplicationCommandTarget::InvocationInfo const & info) override;
    //==============================================================================
    static void handleOpenManual();
    //==============================================================================
    JUCE_LEAK_DETECTOR(MainContentComponent)
};
