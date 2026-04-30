#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

/** Chain - / Chain + chord window (pending-action style): single button navigates after timeout unless partner arrives first (tuner toggle). Message thread only. */
class ChainChordDetector final : private juce::Timer
{
public:
    ChainChordDetector(std::function<void()> onToggleTuner,
                       std::function<void()> onNavigatePreviousChain,
                       std::function<void()> onNavigateNextChain);

    void chainPreviousClicked();
    void chainNextClicked();

private:
    enum class Pending
    {
        None,
        Previous,
        Next
    };

    void timerCallback() override;

    std::function<void()> toggleTuner;
    std::function<void()> navigatePrevious;
    std::function<void()> navigateNext;

    Pending pending { Pending::None };

    static constexpr int chordWindowMs = 200;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainChordDetector)
};

} // namespace forge7
