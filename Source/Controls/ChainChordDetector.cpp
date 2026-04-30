#include "ChainChordDetector.h"

namespace forge7
{

ChainChordDetector::ChainChordDetector(std::function<void()> onToggleTuner,
                                       std::function<void()> onNavigatePreviousChain,
                                       std::function<void()> onNavigateNextChain)
    : toggleTuner(std::move(onToggleTuner))
    , navigatePrevious(std::move(onNavigatePreviousChain))
    , navigateNext(std::move(onNavigateNextChain))
{
}

void ChainChordDetector::chainPreviousClicked()
{
    stopTimer();

    if (pending == Pending::Next)
    {
        pending = Pending::None;

        if (toggleTuner != nullptr)
            toggleTuner();

        return;
    }

    pending = Pending::Previous;
    startTimer(chordWindowMs);
}

void ChainChordDetector::chainNextClicked()
{
    stopTimer();

    if (pending == Pending::Previous)
    {
        pending = Pending::None;

        if (toggleTuner != nullptr)
            toggleTuner();

        return;
    }

    pending = Pending::Next;
    startTimer(chordWindowMs);
}

void ChainChordDetector::timerCallback()
{
    stopTimer();

    if (pending == Pending::Previous)
    {
        pending = Pending::None;

        if (navigatePrevious != nullptr)
            navigatePrevious();
    }
    else if (pending == Pending::Next)
    {
        pending = Pending::None;

        if (navigateNext != nullptr)
            navigateNext();
    }
}

} // namespace forge7
