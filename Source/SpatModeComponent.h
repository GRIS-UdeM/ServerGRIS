#pragma once

#include "MinSizedComponent.hpp"
#include "SpatMode.hpp"

//==============================================================================
class SpatModeComponent final
    : public MinSizedComponent
    , private juce::TextButton::Listener
{
    static constexpr auto NUM_COLS = 2;
    static constexpr auto NUM_ROWS = 2;
    static constexpr auto INNER_PADDING = 1;

public:
    //==============================================================================
    class Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;
        //==============================================================================
        Listener(Listener const &) = delete;
        Listener(Listener &&) = delete;
        Listener & operator=(Listener const &) = delete;
        Listener & operator=(Listener &&) = delete;
        //==============================================================================
        virtual void handleSpatModeChanged(SpatMode spatMode) = 0;

    private:
        //==============================================================================
        JUCE_LEAK_DETECTOR(Listener)
    };

private:
    //==============================================================================
    SpatMode mSpatMode{};
    Listener & mListener;
    juce::OwnedArray<juce::Button> mButtons{};

public:
    //==============================================================================
    explicit SpatModeComponent(Listener & listener);
    ~SpatModeComponent() override = default;
    //==============================================================================
    SpatModeComponent(SpatModeComponent const &) = delete;
    SpatModeComponent(SpatModeComponent &&) = delete;
    SpatModeComponent & operator=(SpatModeComponent const &) = delete;
    SpatModeComponent & operator=(SpatModeComponent &&) = delete;
    //==============================================================================
    void setSpatMode(SpatMode spatMode);
    //==============================================================================
    void buttonClicked(juce::Button * button) override;
    void resized() override;
    [[nodiscard]] int getMinWidth() const noexcept override;
    [[nodiscard]] int getMinHeight() const noexcept override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(SpatModeComponent)
};