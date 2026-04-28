#include "HardwareControlState.h"

#include <juce_core/juce_core.h>

namespace forge7
{

HardwareControlState::HardwareControlState() = default;

float HardwareControlState::getKnobNormalized(const int knobIndex01) const noexcept
{
    if (knobIndex01 < 0 || knobIndex01 > 3)
        return 0.0f;

    return knobs[static_cast<size_t>(knobIndex01)].load(std::memory_order_relaxed);
}

bool HardwareControlState::isAssignButtonDown(const int assignIndex01) const noexcept
{
    if (assignIndex01 < 0 || assignIndex01 > 1)
        return false;

    return assignButtons[static_cast<size_t>(assignIndex01)].load(std::memory_order_relaxed);
}

bool HardwareControlState::isChainPreviousDown() const noexcept
{
    return chainPrevDown.load(std::memory_order_relaxed);
}

bool HardwareControlState::isChainNextDown() const noexcept
{
    return chainNextDown.load(std::memory_order_relaxed);
}

int HardwareControlState::getEncoderDetentDeltaSinceLastPoll() noexcept
{
    return encoderDetentAccumulator.load(std::memory_order_relaxed);
}

void HardwareControlState::clearEncoderDetentAccumulator() noexcept
{
    encoderDetentAccumulator.store(0, std::memory_order_relaxed);
}

void HardwareControlState::applyAbsoluteKnob(const int knobIndex01, const float normalized) noexcept
{
    if (knobIndex01 < 0 || knobIndex01 > 3)
        return;

    knobs[static_cast<size_t>(knobIndex01)].store(juce::jlimit(0.0f, 1.0f, normalized),
                                                     std::memory_order_relaxed);
}

void HardwareControlState::setAssignButton(const int assignIndex01, const bool isDown) noexcept
{
    if (assignIndex01 < 0 || assignIndex01 > 1)
        return;

    assignButtons[static_cast<size_t>(assignIndex01)].store(isDown, std::memory_order_relaxed);
}

void HardwareControlState::setChainPrevious(const bool isDown) noexcept
{
    chainPrevDown.store(isDown, std::memory_order_relaxed);
}

void HardwareControlState::setChainNext(const bool isDown) noexcept
{
    chainNextDown.store(isDown, std::memory_order_relaxed);
}

void HardwareControlState::addEncoderDetents(const int delta) noexcept
{
    if (delta == 0)
        return;

    encoderDetentAccumulator.fetch_add(delta, std::memory_order_relaxed);
}

} // namespace forge7
