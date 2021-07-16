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

#include "GrisLookAndFeel.hpp"
#include "LogicStrucs.hpp"
#include "MinSizedComponent.hpp"
#include "StrongTypes.hpp"

#include <JuceHeader.h>

static dbfs_t constexpr MIN_LEVEL_COMP{ -60.0f };
static dbfs_t constexpr MAX_LEVEL_COMP{ 0.0f };

class GrisLookAndFeel;

//============================ LevelBox ================================
class LevelBox final : public juce::Component
{
    //==============================================================================
    SmallGrisLookAndFeel & mLookAndFeel;

    juce::ColourGradient mColorGrad;
    juce::Image mVuMeterBit;
    juce::Image mVuMeterBackBit;
    juce::Image mVuMeterMutedBit;
    bool mIsClipping{};
    bool mIsMuted{};
    dbfs_t mLevel{ MIN_LEVEL_COMP };

public:
    static constexpr auto MIN_HEIGHT = 140;
    //==============================================================================
    explicit LevelBox(SmallGrisLookAndFeel & lookAndFeel) : mLookAndFeel(lookAndFeel) {}
    ~LevelBox() override = default;
    //==============================================================================
    LevelBox(LevelBox const &) = delete;
    LevelBox(LevelBox &&) = delete;
    LevelBox & operator=(LevelBox const &) = delete;
    LevelBox & operator=(LevelBox &&) = delete;
    //==============================================================================
    void resized() override;
    void resetClipping();
    void setLevel(dbfs_t level);
    void setMuted(bool muted);
    //==============================================================================
    void paint(juce::Graphics & g) override;
    void mouseDown(const juce::MouseEvent & e) override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(LevelBox)
};

//==============================================================================
class AbstractVuMeterComponent
    : public MinSizedComponent
    , public juce::ToggleButton::Listener
{
protected:
    static constexpr auto ID_BUTTON_HEIGHT = 17;
    static constexpr auto MUTE_AND_SOLO_BUTTONS_HEIGHT = 15;
    static constexpr auto INNER_ELEMENTS_PADDING = 1;

    SmallGrisLookAndFeel & mLookAndFeel;

    LevelBox mLevelBox;
    juce::Label mIdLabel;
    juce::TextButton mIdButton;
    juce::TextButton mMuteButton;
    juce::Label mMuteLabel;
    juce::TextButton mSoloButton;
    juce::Label mSoloLabel;

public:
    //==============================================================================
    explicit AbstractVuMeterComponent(juce::String const & id, SmallGrisLookAndFeel & lookAndFeel);
    //==============================================================================
    AbstractVuMeterComponent() = delete;
    ~AbstractVuMeterComponent() override = default;
    //==============================================================================
    AbstractVuMeterComponent(AbstractVuMeterComponent const &) = delete;
    AbstractVuMeterComponent(AbstractVuMeterComponent &&) = delete;
    AbstractVuMeterComponent & operator=(AbstractVuMeterComponent const &) = delete;
    AbstractVuMeterComponent & operator=(AbstractVuMeterComponent &&) = delete;
    //==============================================================================
    void setLevel(dbfs_t const level) { mLevelBox.setLevel(level); }
    void resetClipping() { mLevelBox.resetClipping(); }
    void setState(PortState state, bool soloMode);
    //==============================================================================
    void resized() override;
    [[nodiscard]] int getMinWidth() const noexcept final;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(AbstractVuMeterComponent)
}; // class LevelComponent

//==============================================================================
class SourceVuMeterComponent final
    : public AbstractVuMeterComponent
    , public juce::ChangeListener
{
public:
    static juce::String const NO_DIRECT_OUT_TEXT;
    //==============================================================================
    class Owner
    {
    public:
        virtual ~Owner() = default;
        virtual void setSourceDirectOut(source_index_t sourceIndex, tl::optional<output_patch_t> outputPatch) = 0;
        virtual void setSourceColor(source_index_t sourceIndex, juce::Colour colour) = 0;
        virtual void setSourceState(source_index_t sourceIndex, PortState state) = 0;
        [[nodiscard]] virtual SpeakersData const & getSpeakersData() const = 0;
    };

private:
    static constexpr auto DIRECT_OUT_BUTTON_HEIGHT = 17;
    //==============================================================================
    source_index_t mSourceIndex{};
    juce::TextButton mDirectOutButton;
    Owner & mOwner;

public:
    //==============================================================================
    SourceVuMeterComponent(source_index_t sourceIndex,
                           tl::optional<output_patch_t> directOut,
                           juce::Colour colour,
                           Owner & owner,
                           SmallGrisLookAndFeel & lookAndFeel);
    ~SourceVuMeterComponent() override = default;
    //==============================================================================
    SourceVuMeterComponent(SourceVuMeterComponent const &) = delete;
    SourceVuMeterComponent(SourceVuMeterComponent &&) = delete;
    SourceVuMeterComponent & operator=(SourceVuMeterComponent const &) = delete;
    SourceVuMeterComponent & operator=(SourceVuMeterComponent &&) = delete;
    //==============================================================================
    void setDirectOut(tl::optional<output_patch_t> outputPatch);
    void setSourceColour(juce::Colour colour);
    //==============================================================================
    // overrides
    void buttonClicked(juce::Button * button) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster * source) override;
    [[nodiscard]] int getMinHeight() const noexcept override;
    void mouseUp(const juce::MouseEvent & event) override;

private:
    //==============================================================================
    void muteButtonClicked() const;
    void soloButtonClicked() const;
    void colorSelectorLeftButtonClicked();
    void colorSelectorRightButtonClicked() const;
    void directOutButtonClicked() const;
    [[nodiscard]] juce::Colour getSourceColor() const;
    //==============================================================================
    JUCE_LEAK_DETECTOR(AbstractVuMeterComponent)
}; // class LevelComponent

//==============================================================================
class SpeakerVuMeterComponent final : public AbstractVuMeterComponent
{
public:
    //==============================================================================
    class Owner
    {
    public:
        virtual ~Owner() = default;
        virtual void setSelectedSpeakers(juce::Array<output_patch_t> selection) = 0;
        virtual void setSpeakerState(output_patch_t outputPatch, PortState state) = 0;
    };

private:
    //==============================================================================
    output_patch_t mOutputPatch{};
    Owner & mOwner;

public:
    //==============================================================================
    SpeakerVuMeterComponent(output_patch_t outputPatch, Owner & owner, SmallGrisLookAndFeel & lookAndFeel);
    ~SpeakerVuMeterComponent() override = default;
    //==============================================================================
    SpeakerVuMeterComponent(SpeakerVuMeterComponent const &) = delete;
    SpeakerVuMeterComponent(SpeakerVuMeterComponent &&) = delete;
    SpeakerVuMeterComponent & operator=(SpeakerVuMeterComponent const &) = delete;
    SpeakerVuMeterComponent & operator=(SpeakerVuMeterComponent &&) = delete;
    //==============================================================================
    void setSelected(bool value);
    //==============================================================================
    void buttonClicked(juce::Button * button) override;
    [[nodiscard]] int getMinHeight() const noexcept override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(SpeakerVuMeterComponent)
}; // class LevelComponent

//==============================================================================
class StereoVuMeterComponent final : public AbstractVuMeterComponent
{
public:
    StereoVuMeterComponent(juce::String const & id, SmallGrisLookAndFeel & lookAndFeel);
    ~StereoVuMeterComponent() override = default;

    StereoVuMeterComponent(StereoVuMeterComponent const &) = delete;
    StereoVuMeterComponent(StereoVuMeterComponent &&) = delete;
    StereoVuMeterComponent & operator=(StereoVuMeterComponent const &) = delete;
    StereoVuMeterComponent & operator=(StereoVuMeterComponent &&) = delete;

    [[nodiscard]] int getMinHeight() const noexcept override;
    void buttonClicked(juce::Button *) override {}
};