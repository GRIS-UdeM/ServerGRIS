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

#include "VuMeterComponent.hpp"

#include "LogicStrucs.hpp"
#include "MainComponent.hpp"

auto constexpr VU_METER_COMPONENT_WIDTH = 25;

juce::String const SourceVuMeterComponent::NO_DIRECT_OUT_TEXT = "-";

//==============================================================================
void LevelBox::resized()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const width{ getWidth() };
    auto const height{ getHeight() };

    mColorGrad = juce::ColourGradient{ juce::Colour::fromRGB(255, 94, 69),
                                       0.f,
                                       0.f,
                                       juce::Colour::fromRGB(17, 255, 159),
                                       0.f,
                                       narrow<float>(height),
                                       false };
    mColorGrad.addColour(0.1, juce::Colours::yellow);

    // Create vu-meter foreground image.
    mVuMeterBit = juce::Image{ juce::Image::RGB, width, height, true }; // used to be width - 1
    juce::Graphics gf{ mVuMeterBit };
    gf.setGradientFill(mColorGrad);
    gf.fillRect(0, 0, getWidth(), getHeight());
    gf.setColour(mLookAndFeel.getDarkColour());
    gf.setFont(10.0f);

    // Create vu-meter background image.
    mVuMeterBackBit = juce::Image{ juce::Image::RGB, width, height, true }; // used to be width - 1
    juce::Graphics gb{ mVuMeterBackBit };
    gb.setColour(mLookAndFeel.getDarkColour());
    gb.fillRect(0, 0, getWidth(), getHeight());
    gb.setColour(mLookAndFeel.getScrollBarColour());
    gb.setFont(10.0f);

    // Create vu-meter muted image.
    mVuMeterMutedBit = juce::Image(juce::Image::RGB, width, height, true); // used to be width - 1
    juce::Graphics gm{ mVuMeterMutedBit };
    gm.setColour(mLookAndFeel.getWinBackgroundColour());
    gm.fillRect(0, 0, getWidth(), getHeight());
    gm.setColour(mLookAndFeel.getScrollBarColour());
    gm.setFont(10.0f);

    // Draw ticks on images.
    auto const start = getWidth() - 3;
    static constexpr auto NUM_TICKS = 10;
    for (int i{ 1 }; i < NUM_TICKS; i++) {
        auto const y = i * height / NUM_TICKS;
        auto const y_f{ narrow<float>(y) };
        auto const start_f{ narrow<float>(start) };
        auto const with_f{ narrow<float>(getWidth()) };

        gf.drawLine(start_f, y_f, with_f, y_f, 1.0f);
        gb.drawLine(start_f, y_f, with_f, y_f, 1.0f);
        gm.drawLine(start_f, y_f, with_f, y_f, 1.0f);
        if (i % 2 == 1) {
            gf.drawText(juce::String(i * -6), start - 15, y - 5, 15, 10, juce::Justification::centred, false);
            gb.drawText(juce::String(i * -6), start - 15, y - 5, 15, 10, juce::Justification::centred, false);
            gm.drawText(juce::String(i * -6), start - 15, y - 5, 15, 10, juce::Justification::centred, false);
        }
    }
}

//==============================================================================
void LevelBox::paint(juce::Graphics & g)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const width{ getWidth() };
    auto const height{ getHeight() };

    if (mIsMuted) {
        g.drawImage(mVuMeterMutedBit, 0, 0, width, height, 0, 0, width, height);
        return;
    }

    if (mLevel <= MIN_LEVEL_COMP && !mIsClipping) {
        g.drawImage(mVuMeterBackBit, 0, 0, width, height, 0, 0, width, height);
        return;
    }

    auto const magnitude{ 1.0f - std::clamp(mLevel, MIN_LEVEL_COMP, MAX_LEVEL_COMP) / MIN_LEVEL_COMP };
    auto const rel = narrow<int>(std::round(magnitude * narrow<float>(height)));
    auto const h = height - rel;
    g.drawImage(mVuMeterBit, 0, h, width, rel, 0, h, width, rel);
    g.drawImage(mVuMeterBackBit, 0, 0, width, h, 0, 0, width, h);
    if (mIsClipping) {
        g.setColour(juce::Colour::fromHSV(0.0, 1, 0.75, 1));
        juce::Rectangle<float> const clipRect{ 0.5, 0.5, narrow<float>(height - 1), 5 };
        g.fillRect(clipRect);
    }
}

//==============================================================================
void LevelBox::mouseDown(juce::MouseEvent const & e)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    juce::Rectangle<int> const hitBox{ 0, 0, getWidth(), 20 };
    if (hitBox.contains(e.getPosition())) {
        resetClipping();
    }
}

//==============================================================================
void LevelBox::resetClipping()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mIsClipping = false;
    repaint();
}

//==============================================================================
void LevelBox::setLevel(dbfs_t const level)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const & clippedLevel{ std::clamp(level, MIN_LEVEL_COMP, MAX_LEVEL_COMP) };

    if (clippedLevel == mLevel) {
        return;
    }

    if (level > MAX_LEVEL_COMP) {
        mIsClipping = true;
    }
    mLevel = clippedLevel;

    repaint();
}

//==============================================================================
void LevelBox::setMuted(bool const muted)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (muted == mIsMuted) {
        return;
    }
    mIsMuted = muted;
    repaint();
}

//==============================================================================
AbstractVuMeterComponent::AbstractVuMeterComponent(juce::String const & id, SmallGrisLookAndFeel & lookAndFeel)
    : mLookAndFeel(lookAndFeel)
    , mLevelBox(lookAndFeel)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const initColors = [&](juce::Component & component) {
        component.setLookAndFeel(&lookAndFeel);
        component.setColour(juce::Label::textColourId, lookAndFeel.getFontColour());
        component.setColour(juce::TextButton::textColourOnId, lookAndFeel.getFontColour());
        component.setColour(juce::TextButton::textColourOffId, lookAndFeel.getFontColour());
        component.setColour(juce::TextButton::buttonColourId, lookAndFeel.getBackgroundColour());
        addAndMakeVisible(component);
    };

    auto const initButton = [&](juce::Button & button) {
        button.addListener(this);
        button.addMouseListener(this, true);
        initColors(button);
    };

    auto const initLabel = [&](juce::Label & label, juce::String const & text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);
        initColors(label);
        label.setFont(juce::Font{ 1.0f });
    };

    // Id
    initButton(mIdButton);
    initLabel(mIdLabel, id);

    // Mute button
    mMuteButton.setClickingTogglesState(true);
    initButton(mMuteButton);

    // Mute label
    initLabel(mMuteLabel, "m");

    // Solo button
    mSoloButton.setClickingTogglesState(true);
    initButton(mSoloButton);

    // Solo label
    initLabel(mSoloLabel, "s");

    // Level box
    addAndMakeVisible(mLevelBox);
}

//==============================================================================
void AbstractVuMeterComponent::setState(PortState const state, bool const soloMode)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mSoloButton.setToggleState(state == PortState::solo, juce::dontSendNotification);
    mMuteButton.setToggleState(state == PortState::muted, juce::dontSendNotification);
    mLevelBox.setMuted(soloMode ? state != PortState::solo : state == PortState::muted);

    repaint();
}

//==============================================================================
void SpeakerVuMeterComponent::setSelected(bool const value)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (value) {
        mIdButton.setColour(juce::TextButton::textColourOnId, mLookAndFeel.getWinBackgroundColour());
        mIdButton.setColour(juce::TextButton::textColourOffId, mLookAndFeel.getWinBackgroundColour());
        mIdButton.setColour(juce::TextButton::buttonColourId, mLookAndFeel.getOnColour());
    } else {
        mIdButton.setColour(juce::TextButton::textColourOnId, mLookAndFeel.getFontColour());
        mIdButton.setColour(juce::TextButton::textColourOffId, mLookAndFeel.getFontColour());
        mIdButton.setColour(juce::TextButton::buttonColourId, mLookAndFeel.getBackgroundColour());
    }
    repaint();
}

//==============================================================================
void SpeakerVuMeterComponent::buttonClicked(juce::Button * button)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (button == &mMuteButton) {
        auto const newState{ mMuteButton.getToggleState() ? PortState::muted : PortState::normal };
        mOwner.setSpeakerState(mOutputPatch, newState);
    } else if (button == &mSoloButton) {
        auto const newState{ mSoloButton.getToggleState() ? PortState::solo : PortState::normal };
        mOwner.setSpeakerState(mOutputPatch, newState);
    } else if (button == &mIdButton) {
        mOwner.setSelectedSpeakers(mOutputPatch);
    }
}

//==============================================================================
int SpeakerVuMeterComponent::getMinHeight() const noexcept
{
    return INNER_ELEMENTS_PADDING + ID_BUTTON_HEIGHT + INNER_ELEMENTS_PADDING + LevelBox::MIN_HEIGHT
           + INNER_ELEMENTS_PADDING + MUTE_AND_SOLO_BUTTONS_HEIGHT + INNER_ELEMENTS_PADDING;
}

//==============================================================================
StereoVuMeterComponent::StereoVuMeterComponent(juce::String const & id, SmallGrisLookAndFeel & lookAndFeel)
    : AbstractVuMeterComponent(id, lookAndFeel)
{
}

//==============================================================================
int StereoVuMeterComponent::getMinHeight() const noexcept
{
    return INNER_ELEMENTS_PADDING + ID_BUTTON_HEIGHT + INNER_ELEMENTS_PADDING + LevelBox::MIN_HEIGHT
           + INNER_ELEMENTS_PADDING;
}

//==============================================================================
void AbstractVuMeterComponent::resized()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto yOffset{ INNER_ELEMENTS_PADDING };
    static constexpr auto AVAILABLE_WIDTH{ VU_METER_COMPONENT_WIDTH - INNER_ELEMENTS_PADDING * 2 };

    juce::Rectangle<int> const idBounds{ INNER_ELEMENTS_PADDING, yOffset, AVAILABLE_WIDTH, ID_BUTTON_HEIGHT };
    mIdLabel.setBounds(idBounds.withSizeKeepingCentre(100, 100));
    mIdButton.setBounds(idBounds);

    yOffset += ID_BUTTON_HEIGHT + INNER_ELEMENTS_PADDING;

    auto const vuMeterHeight{ std::max(LevelBox::MIN_HEIGHT, getHeight() - getMinHeight() + LevelBox::MIN_HEIGHT) };

    juce::Rectangle<int> const levelBoxBounds{ INNER_ELEMENTS_PADDING, yOffset, AVAILABLE_WIDTH, vuMeterHeight };
    mLevelBox.setBounds(levelBoxBounds);

    yOffset += vuMeterHeight + INNER_ELEMENTS_PADDING;

    static constexpr auto MUTE_AND_SOLO_WIDTH{ (AVAILABLE_WIDTH - INNER_ELEMENTS_PADDING) / 2 };

    juce::Rectangle<int> const muteButtonBounds{ INNER_ELEMENTS_PADDING,
                                                 yOffset,
                                                 MUTE_AND_SOLO_WIDTH,
                                                 MUTE_AND_SOLO_BUTTONS_HEIGHT };
    mMuteButton.setBounds(muteButtonBounds);
    mMuteLabel.setBounds(muteButtonBounds.withSizeKeepingCentre(100, 100));

    juce::Rectangle<int> const soloButtonBounds{ INNER_ELEMENTS_PADDING * 2 + MUTE_AND_SOLO_WIDTH,
                                                 yOffset,
                                                 MUTE_AND_SOLO_WIDTH,
                                                 MUTE_AND_SOLO_BUTTONS_HEIGHT };
    mSoloButton.setBounds(soloButtonBounds);
    mSoloLabel.setBounds(soloButtonBounds.withSizeKeepingCentre(100, 100));
}

//==============================================================================
int AbstractVuMeterComponent::getMinWidth() const noexcept
{
    return VU_METER_COMPONENT_WIDTH;
}

//==============================================================================
SourceVuMeterComponent::SourceVuMeterComponent(source_index_t const sourceIndex,
                                               tl::optional<output_patch_t> const directOut,
                                               juce::Colour const colour,
                                               Owner & owner,
                                               SmallGrisLookAndFeel & lookAndFeel)
    : AbstractVuMeterComponent(juce::String{ sourceIndex.get() }, lookAndFeel)
    , mSourceIndex(sourceIndex)
    , mOwner(owner)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mDirectOutButton.setColour(juce::Label::textColourId, lookAndFeel.getFontColour());
    mDirectOutButton.setLookAndFeel(&lookAndFeel);
    mDirectOutButton.addListener(this);
    mDirectOutButton.addMouseListener(this, true);
    mIdButton.addMouseListener(this, true);
    setDirectOut(directOut);
    addAndMakeVisible(mDirectOutButton);

    setSourceColour(colour);
}

//==============================================================================
void SourceVuMeterComponent::setDirectOut(tl::optional<output_patch_t> const outputPatch)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    // TODO : the following code always results in some app-crashing invalid string when the optional holds a value. Why
    // is that ? Is it possible that the perfect-forwarding of a juce::String does something weird ?

    /*static auto const PATCH_TO_STRING
        = [](output_patch_t const outputPatch) -> juce::String { return juce::String{ outputPatch.get() }; };

    auto const newText{ outputPatch.map_or(PATCH_TO_STRING, NO_DIRECT_OUT_TEXT) };*/

    auto const newText{ outputPatch ? juce::String{ outputPatch->get() } : NO_DIRECT_OUT_TEXT };
    mDirectOutButton.setButtonText(newText);
}

//==============================================================================
void SourceVuMeterComponent::setSourceColour(juce::Colour const colour)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    mIdButton.setColour(juce::TextButton::buttonColourId, colour);
}

//==============================================================================
void SourceVuMeterComponent::buttonClicked(juce::Button * button)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (button == &mMuteButton) {
        muteButtonClicked();
    } else if (button == &mSoloButton) {
        soloButtonClicked();
    } else if (button == &mDirectOutButton) {
        directOutButtonClicked();
    }
}

//==============================================================================
void SourceVuMeterComponent::resized()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    AbstractVuMeterComponent::resized();

    juce::Rectangle<int> const directOutButtonBounds{ INNER_ELEMENTS_PADDING,
                                                      getHeight() - (DIRECT_OUT_BUTTON_HEIGHT + INNER_ELEMENTS_PADDING),
                                                      VU_METER_COMPONENT_WIDTH - INNER_ELEMENTS_PADDING * 2,
                                                      DIRECT_OUT_BUTTON_HEIGHT };
    mDirectOutButton.setBounds(directOutButtonBounds);
}

//==============================================================================
void SourceVuMeterComponent::changeListenerCallback(juce::ChangeBroadcaster * source)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto * colorSelector{ dynamic_cast<juce::ColourSelector *>(source) };
    jassert(colorSelector);
    if (colorSelector != nullptr) {
        mOwner.setSourceColor(mSourceIndex, colorSelector->getCurrentColour());
    }
}

//==============================================================================
int SourceVuMeterComponent::getMinHeight() const noexcept
{
    return INNER_ELEMENTS_PADDING + ID_BUTTON_HEIGHT + INNER_ELEMENTS_PADDING + LevelBox::MIN_HEIGHT
           + INNER_ELEMENTS_PADDING + MUTE_AND_SOLO_BUTTONS_HEIGHT + INNER_ELEMENTS_PADDING + DIRECT_OUT_BUTTON_HEIGHT
           + INNER_ELEMENTS_PADDING;
}

//==============================================================================
void SourceVuMeterComponent::mouseUp(juce::MouseEvent const & event)
{
    if (!mIdButton.getScreenBounds().contains(event.getScreenPosition())) {
        return;
    }

    if (event.mods.isLeftButtonDown()) {
        colorSelectorLeftButtonClicked();
    } else if (event.mods.isRightButtonDown()) {
        colorSelectorRightButtonClicked();
    }
}

//==============================================================================
void SourceVuMeterComponent::muteButtonClicked() const
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const newState{ mMuteButton.getToggleState() ? PortState::muted : PortState::normal };
    mOwner.setSourceState(mSourceIndex, newState);
}

//==============================================================================
void SourceVuMeterComponent::soloButtonClicked() const
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto const newState{ mSoloButton.getToggleState() ? PortState::solo : PortState::normal };
    mOwner.setSourceState(mSourceIndex, newState);
}

//==============================================================================
void SourceVuMeterComponent::colorSelectorLeftButtonClicked()
{
    JUCE_ASSERT_MESSAGE_THREAD;

    auto colourSelector{ std::make_unique<juce::ColourSelector>(juce::ColourSelector::showColourAtTop
                                                                    | juce::ColourSelector::showSliders
                                                                    | juce::ColourSelector::showColourspace,
                                                                4,
                                                                4) };
    colourSelector->setName("background");
    colourSelector->setCurrentColour(getSourceColor());
    colourSelector->addChangeListener(this);
    colourSelector->setColour(juce::ColourSelector::backgroundColourId, juce::Colours::transparentBlack);
    colourSelector->setSize(300, 400);
    juce::CallOutBox::launchAsynchronously(std::move(colourSelector), getScreenBounds(), nullptr);
}

//==============================================================================
void SourceVuMeterComponent::colorSelectorRightButtonClicked() const
{
    source_index_t const nextSourceIndex{ mSourceIndex.get() + 1 };
    auto const currentColor{ getSourceColor() };
    mOwner.setSourceColor(nextSourceIndex, currentColor);
}

//==============================================================================
void SourceVuMeterComponent::directOutButtonClicked() const
{
    JUCE_ASSERT_MESSAGE_THREAD;

    static constexpr auto CHOICE_NOT_DIRECT_OUT = std::numeric_limits<int>::min();
    static constexpr auto CHOICE_CANCELED = 0;

    juce::PopupMenu menu{};
    juce::Array<output_patch_t> directOutSpeakers{};
    juce::Array<output_patch_t> nonDirectOutSpeakers{};
    for (auto const speaker : mOwner.getSpeakersData()) {
        auto & destination{ speaker.value->isDirectOutOnly ? directOutSpeakers : nonDirectOutSpeakers };
        destination.add(speaker.key);
    }
    for (auto const outputPatch : directOutSpeakers) {
        menu.addItem(outputPatch.get(), juce::String{ outputPatch.get() });
    }
    menu.addItem(CHOICE_NOT_DIRECT_OUT, NO_DIRECT_OUT_TEXT);
    for (auto const outputPatch : nonDirectOutSpeakers) {
        menu.addItem(outputPatch.get(), juce::String{ outputPatch.get() });
    }

    auto const result{ menu.show() };

    if (result == CHOICE_CANCELED) {
        return;
    }

    tl::optional<output_patch_t> newOutputPatch{};
    if (result != CHOICE_NOT_DIRECT_OUT) {
        newOutputPatch = output_patch_t{ result };
    }

    mOwner.setSourceDirectOut(mSourceIndex, newOutputPatch);
}

//==============================================================================
juce::Colour SourceVuMeterComponent::getSourceColor() const
{
    return mIdButton.findColour(juce::TextButton::buttonColourId);
}

//==============================================================================
SpeakerVuMeterComponent::SpeakerVuMeterComponent(output_patch_t const outputPatch,
                                                 Owner & owner,
                                                 SmallGrisLookAndFeel & lookAndFeel)
    : AbstractVuMeterComponent(juce::String{ outputPatch.get() }, lookAndFeel)
    , mOutputPatch(outputPatch)
    , mOwner(owner)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    setSelected(false);
}
