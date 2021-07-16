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

#include "ControlPanel.hpp"

#include "MainComponent.hpp"

static constexpr auto COL_1_WIDTH = 100;
static constexpr auto COL_2_WIDTH = 160;
static constexpr auto ROW_1_CONTENT_HEIGHT = 20;
static constexpr auto ROW_2_CONTENT_HEIGHT = 20;

static constexpr auto COL_PADDING = 5;
static constexpr auto ROW_PADDING = 5;

static constexpr auto LABEL_HEIGHT = 18;
static constexpr auto COL_INNER_PADDING = 3;

//==============================================================================
GainsSubPanel::GainsSubPanel(MainContentComponent & mainContentComponent, GrisLookAndFeel & lookAndFeel)
    : SubPanelComponent(LayoutComponent::Orientation::horizontal, lookAndFeel)
    , mMainContentComponent(mainContentComponent)
    , mLookAndFeel(lookAndFeel)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    addSection(mMasterGainSlider).withChildMinSize();
    addSection(mInterpolationSlider).withChildMinSize();
}

//==============================================================================
void GainsSubPanel::setMasterGain(dbfs_t const gain)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mMasterGainSlider.setValue(gain.get());
}

//==============================================================================
void GainsSubPanel::setInterpolation(float const interpolation)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mInterpolationSlider.setValue(interpolation);
}

//==============================================================================
void GainsSubPanel::sliderMoved(float const value, SpatSlider * const slider)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (slider == &mMasterGainSlider) {
        mMainContentComponent.masterGainChanged(dbfs_t{ value });
        return;
    }

    jassert(slider == &mInterpolationSlider);
    mMainContentComponent.interpolationChanged(value);
}

//==============================================================================
SpatSettingsSubPanel::SpatSettingsSubPanel(ControlPanel & controlPanel,
                                           MainContentComponent & mainContentComponent,
                                           GrisLookAndFeel & lookAndFeel)
    : SubPanelComponent(LayoutComponent::Orientation::horizontal, lookAndFeel)
    , mControlPanel(controlPanel)
    , mMainContentComponent(mainContentComponent)
    , mLookAndFeel(lookAndFeel)
    , mAttenuationValues(getAttenuationValues())
{
    auto const initLabel = [&](juce::Label & label,
                               juce::String const & text,
                               juce::Justification justification = juce::Justification::centredTop) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(justification);
        label.setColour(juce::Label::ColourIds::textColourId, lookAndFeel.getFontColour());
    };

    auto const initButton = [&](juce::Button & button, juce::String const & text, juce::String const & tooltip) {
        button.setClickingTogglesState(true);
        button.setRadioGroupId(SPAT_MODE_BUTTONS_RADIO_GROUP_ID, juce::dontSendNotification);
        button.setButtonText(text);
        button.setTooltip(tooltip);
        button.addListener(this);
    };

    initLabel(mAlgorithmSelectionLabel, "Algorithm selection");
    initLabel(mAttenuationSettingsLabel, "Attenuation settings");
    initLabel(mStereoReductionLabel, "Stereo reduction");
    initLabel(mStereoRoutingLabel, "Stereo routing");
    initLabel(mLeftLabel, "Left :", juce::Justification::topRight);
    initLabel(mRightLabel, "Right :", juce::Justification::topRight);

    initButton(mDomeButton, spatModeToString(SpatMode::vbap), spatModeToTooltip(SpatMode::vbap));
    initButton(mCubeButton, spatModeToString(SpatMode::lbap), spatModeToTooltip(SpatMode::lbap));

    juce::StringArray items{ "None" };
    items.addArray(STEREO_MODE_STRINGS);
    mStereoReductionCombo.addItemList(items, 1);
    mStereoReductionCombo.addListener(this);

    mAttenuationDbCombo.addItemList(mAttenuationValues.dbStrings, 1);
    mAttenuationDbCombo.addListener(this);

    mAttenuationHzCombo.addItemList(mAttenuationValues.hzStrings, 1);
    mAttenuationHzCombo.addListener(this);

    mLeftCombo.addListener(this);
    mRightCombo.addListener(this);

    mDomeButton.setToggleState(true, juce::dontSendNotification);

    static constexpr auto ALGORITHM_BUTTONS_WIDTH = (COL_1_WIDTH - COL_INNER_PADDING) / 2;
    mAlgorithmButtonsLayout.addSection(mDomeButton)
        .withFixedSize(ALGORITHM_BUTTONS_WIDTH)
        .withRightPadding(COL_INNER_PADDING);
    mAlgorithmButtonsLayout.addSection(mCubeButton).withFixedSize(ALGORITHM_BUTTONS_WIDTH);

    mCol1Layout.addSection(mAlgorithmSelectionLabel).withFixedSize(LABEL_HEIGHT);
    mCol1Layout.addSection(mAlgorithmButtonsLayout).withFixedSize(ROW_1_CONTENT_HEIGHT).withBottomPadding(ROW_PADDING);
    mCol1Layout.addSection(mStereoReductionLabel).withFixedSize(LABEL_HEIGHT);
    mCol1Layout.addSection(mStereoReductionCombo).withFixedSize(ROW_2_CONTENT_HEIGHT);

    static constexpr auto COL_2_HALF_WIDTH = (COL_2_WIDTH - COL_INNER_PADDING) / 2;
    mAttenuationLayout.addSection(mAttenuationDbCombo)
        .withFixedSize(COL_2_HALF_WIDTH)
        .withRightPadding(COL_INNER_PADDING);
    mAttenuationLayout.addSection(mAttenuationHzCombo).withFixedSize(COL_2_HALF_WIDTH);

    static constexpr auto COL_2_QUARTER_WIDTH = COL_2_HALF_WIDTH / 2;
    mStereoRoutingLayout.clearSections();
    mStereoRoutingLayout.addSection(mLeftLabel).withFixedSize(COL_2_QUARTER_WIDTH);
    mStereoRoutingLayout.addSection(mLeftCombo).withFixedSize(COL_2_QUARTER_WIDTH).withRightPadding(COL_INNER_PADDING);
    mStereoRoutingLayout.addSection(mRightLabel).withFixedSize(COL_2_QUARTER_WIDTH);
    mStereoRoutingLayout.addSection(mRightCombo).withFixedSize(COL_2_QUARTER_WIDTH);

    mCol2Layout.addSection(mAttenuationSettingsLabel).withFixedSize(LABEL_HEIGHT);
    mCol2Layout.addSection(mAttenuationLayout).withFixedSize(ROW_1_CONTENT_HEIGHT).withBottomPadding(ROW_PADDING);
    mCol2Layout.addSection(mStereoRoutingLabel).withFixedSize(LABEL_HEIGHT);
    mCol2Layout.addSection(mStereoRoutingLayout).withFixedSize(ROW_2_CONTENT_HEIGHT);

    updateLayout();
}

//==============================================================================
SpatMode SpatSettingsSubPanel::getSpatMode() const
{
    if (mDomeButton.getToggleState()) {
        return SpatMode::vbap;
    }

    jassert(mCubeButton.getToggleState());
    return SpatMode::lbap;
}

//==============================================================================
tl::optional<StereoMode> SpatSettingsSubPanel::getStereoMode() const
{
    JUCE_ASSERT_MESSAGE_THREAD;
    return stringToStereoMode(mStereoReductionCombo.getText());
}

//==============================================================================
bool SpatSettingsSubPanel::shouldShowAttenuationSettings() const
{
    return getSpatMode() == SpatMode::lbap;
}

//==============================================================================
bool SpatSettingsSubPanel::shouldShowStereoRouting() const
{
    return getStereoMode().has_value();
}

//==============================================================================
void SpatSettingsSubPanel::updateSpeakers(SpeakersOrdering speakers, StereoRouting const & routing)
{
    speakers.sort();

    mLeftCombo.clear(juce::dontSendNotification);
    mRightCombo.clear(juce::dontSendNotification);

    for (auto const & outputPatch : speakers) {
        auto const outputPatchInt{ outputPatch.get() };
        mLeftCombo.addItem(juce::String{ outputPatchInt }, outputPatchInt);
        mRightCombo.addItem(juce::String{ outputPatchInt }, outputPatchInt);
    }

    mLeftCombo.setSelectedId(routing.left.get(), juce::dontSendNotification);
    mRightCombo.setSelectedId(routing.right.get(), juce::dontSendNotification);

    updateEnabledStereoRoutings();
}

//==============================================================================
void SpatSettingsSubPanel::setSpatMode(SpatMode const spatMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    switch (spatMode) {
    case SpatMode::vbap:
        mDomeButton.setToggleState(true, juce::dontSendNotification);
        break;
    case SpatMode::lbap:
        mCubeButton.setToggleState(true, juce::dontSendNotification);
        break;
    default:
        jassertfalse;
    }
    updateLayout();
    mControlPanel.forceLayoutUpdate();
}

//==============================================================================
void SpatSettingsSubPanel::setStereoMode(tl::optional<StereoMode> const & stereoMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    if (!stereoMode) {
        mStereoReductionCombo.setSelectedItemIndex(0, juce::dontSendNotification);
    } else {
        auto const index{ static_cast<int>(*stereoMode) + 1 };
        mStereoReductionCombo.setSelectedItemIndex(index, juce::dontSendNotification);
    }
    updateLayout();
    mControlPanel.forceLayoutUpdate();
}

//==============================================================================
void SpatSettingsSubPanel::setAttenuationDb(dbfs_t const attenuation)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const index{ mAttenuationValues.dbValues.indexOf(attenuation) };
    mAttenuationDbCombo.setSelectedItemIndex(index, juce::dontSendNotification);
}

//==============================================================================
void SpatSettingsSubPanel::setAttenuationHz(hz_t const freq)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const index{ mAttenuationValues.hzValues.indexOf(freq) };
    mAttenuationHzCombo.setSelectedItemIndex(index);
}

//==============================================================================
void SpatSettingsSubPanel::setStereoRouting(StereoRouting const & routing)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    jassert(routing.left != routing.right);

    mLeftCombo.setSelectedId(routing.left.get(), juce::dontSendNotification);
    mRightCombo.setSelectedId(routing.right.get(), juce::dontSendNotification);

    updateEnabledStereoRoutings();
}

//==============================================================================
void SpatSettingsSubPanel::updateEnabledStereoRoutings()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const leftId{ mLeftCombo.getSelectedId() };
    auto const rightId{ mRightCombo.getSelectedId() };

    jassert(mLeftCombo.getNumItems() == mRightCombo.getNumItems());
    jassert((leftId == 0 && rightId == 0) || (mLeftCombo.getSelectedId() != mRightCombo.getSelectedId()));

    for (int i{}; i < mLeftCombo.getNumItems(); ++i) {
        auto const itemId{ mLeftCombo.getItemId(i) };
        auto const leftEnabled{ itemId != rightId };
        auto const rightEnabled{ itemId != leftId };
        mLeftCombo.setItemEnabled(itemId, leftEnabled);
        mRightCombo.setItemEnabled(itemId, rightEnabled);
    }
}

//==============================================================================
void SpatSettingsSubPanel::updateVisibility()
{
    auto const showAttenuation{ shouldShowAttenuationSettings() };

    mAttenuationSettingsLabel.setVisible(showAttenuation);
    mAttenuationDbCombo.setVisible(showAttenuation);
    mAttenuationHzCombo.setVisible(shouldShowAttenuationSettings());

    auto const showRouting{ shouldShowStereoRouting() };

    mStereoRoutingLabel.setVisible(showRouting);
    mLeftLabel.setVisible(showRouting);
    mLeftCombo.setVisible(showRouting);
    mRightLabel.setVisible(showRouting);
    mRightCombo.setVisible(showRouting);
}

//==============================================================================
void SpatSettingsSubPanel::updateLayout()
{
    auto const showAttenuation{ shouldShowAttenuationSettings() };
    auto const showRouting{ shouldShowStereoRouting() };

    mAttenuationSettingsLabel.setVisible(showAttenuation);
    mAttenuationDbCombo.setVisible(showAttenuation);
    mAttenuationHzCombo.setVisible(showAttenuation);

    mStereoRoutingLabel.setVisible(showRouting);
    mLeftLabel.setVisible(showRouting);
    mLeftCombo.setVisible(showRouting);
    mRightLabel.setVisible(showRouting);
    mRightCombo.setVisible(showRouting);

    clearSections();
    addSection(mCol1Layout).withFixedSize(COL_1_WIDTH).withHorizontalPadding(COL_PADDING);
    if (showAttenuation || showRouting) {
        addSection(mCol2Layout).withFixedSize(COL_2_WIDTH).withLeftPadding(COL_PADDING);
    }
}

//==============================================================================
void SpatSettingsSubPanel::buttonClicked(juce::Button * button)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (!button->getToggleState()) {
        return;
    }

    jassert(button == &mDomeButton || button == &mCubeButton);
    auto const spatMode{ button == &mDomeButton ? SpatMode::vbap : SpatMode::lbap };
    mMainContentComponent.setSpatMode(spatMode);
    updateLayout();
    mControlPanel.forceLayoutUpdate();
}

//==============================================================================
void SpatSettingsSubPanel::comboBoxChanged(juce::ComboBox * comboBoxThatHasChanged)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (comboBoxThatHasChanged == &mAttenuationDbCombo) {
        auto const index{ std::max(mAttenuationDbCombo.getSelectedItemIndex(), 0) };
        auto const attenuation{ mAttenuationValues.dbValues[index] };
        mMainContentComponent.cubeAttenuationDbChanged(attenuation);

        return;
    }

    if (comboBoxThatHasChanged == &mAttenuationHzCombo) {
        auto const index{ std::max(mAttenuationHzCombo.getSelectedItemIndex(), 0) };
        auto const freq{ mAttenuationValues.hzValues[index] };
        mMainContentComponent.cubeAttenuationHzChanged(freq);

        return;
    }

    if (comboBoxThatHasChanged == &mStereoReductionCombo) {
        auto const stereoMode{ getStereoMode() };
        mMainContentComponent.setStereoMode(stereoMode);
        updateLayout();
        mControlPanel.forceLayoutUpdate();

        return;
    }

    if (comboBoxThatHasChanged == &mLeftCombo || comboBoxThatHasChanged == &mRightCombo) {
        StereoRouting const routing{ output_patch_t{ mLeftCombo.getSelectedId() },
                                     output_patch_t{ mRightCombo.getSelectedId() } };
        jassert(routing.left != routing.right);

        mMainContentComponent.setStereoRouting(routing);
        updateEnabledStereoRoutings();

        return;
    }

    jassertfalse;
}

//==============================================================================
SpatSettingsSubPanel::AttenuationValues SpatSettingsSubPanel::getAttenuationValues()
{
    AttenuationValues result{};

    for (auto const & string : ATTENUATION_DB_STRINGS) {
        result.dbValues.add(dbfs_t{ string.getFloatValue() });
        result.dbStrings.add(string + " db");
    }

    for (auto const & string : ATTENUATION_FREQUENCY_STRINGS) {
        result.hzValues.add(hz_t{ string.getFloatValue() });
        result.hzStrings.add(string + " Hz");
    }

    return result;
}

//==============================================================================
ControlPanel::ControlPanel(MainContentComponent & mainContentComponent, GrisLookAndFeel & lookAndFeel)
    : mMainContentComponent(mainContentComponent)
    , mLookAndFeel(lookAndFeel)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    static constexpr auto PADDING = 4;

    mLayout.addSection(mGainsSubPanel).withChildMinSize().withPadding(PADDING);
    mLayout.addSection(mSpatSettingsSubPanel).withChildMinSize().withTopPadding(PADDING).withBottomPadding(PADDING);
    mLayout.addSection(nullptr).withRelativeSize(1.0f);
    mLayout.addSection(mRecordButton).withChildMinSize().withLeftPadding(PADDING).withRightPadding(30);
    mLayout.resized();

    addAndMakeVisible(mLayout);
}

//==============================================================================
void ControlPanel::setMasterGain(dbfs_t const gain)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mGainsSubPanel.setMasterGain(gain);
}

//==============================================================================
void ControlPanel::setInterpolation(float const interpolation)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mGainsSubPanel.setInterpolation(interpolation);
}

//==============================================================================
void ControlPanel::setSpatMode(SpatMode const spatMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mSpatSettingsSubPanel.setSpatMode(spatMode);
}

//==============================================================================
void ControlPanel::setStereoMode(tl::optional<StereoMode> const & mode)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mSpatSettingsSubPanel.setStereoMode(mode);
}

//==============================================================================
void ControlPanel::setCubeAttenuationDb(dbfs_t const value)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mSpatSettingsSubPanel.setAttenuationDb(value);
}

//==============================================================================
void ControlPanel::setCubeAttenuationHz(hz_t const value)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mSpatSettingsSubPanel.setAttenuationHz(value);
}

//==============================================================================
void ControlPanel::setRecordButtonState(RecordButton::State const state)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mRecordButton.setState(state);
}

//==============================================================================
void ControlPanel::setStereoRouting(StereoRouting const & routing)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mSpatSettingsSubPanel.setStereoRouting(routing);
}

//==============================================================================
void ControlPanel::updateSpeakers(SpeakersOrdering ordering, StereoRouting const & routing)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mSpatSettingsSubPanel.updateSpeakers(std::move(ordering), routing);
}

//==============================================================================
void ControlPanel::resized()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mLayout.setBounds(0, 0, getWidth(), getHeight());
}

//==============================================================================
void ControlPanel::forceLayoutUpdate()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    resized();
    mLayout.resized();
}

//==============================================================================
void ControlPanel::recordButtonPressed()
{
    JUCE_ASSERT_MESSAGE_THREAD;
    mMainContentComponent.recordButtonPressed();
}
