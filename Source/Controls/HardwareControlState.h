#pragma once

#include <atomic>
#include <cstdint>

namespace forge7
{

/** Thread-safe mirror of physical control state for GUI / HUD (atomics so audio-adjacent threads
    can read without taking the control manager's mutex).

    Updated only from `ControlManager::submitHardwareEvent` (after normalization), never from raw drivers. */
class HardwareControlState
{
public:
    HardwareControlState();
    ~HardwareControlState() = default;

    float getKnobNormalized(int knobIndex01) const noexcept;
    bool isAssignButtonDown(int assignIndex01) const noexcept;
    bool isChainPreviousDown() const noexcept;
    bool isChainNextDown() const noexcept;

    /** Accumulated encoder detents since last poll (optional UI use); may be cleared by UI. */
    int getEncoderDetentDeltaSinceLastPoll() noexcept;

    /** Called from UI after reading detent delta if one-shot display is desired. */
    void clearEncoderDetentAccumulator() noexcept;

private:
    friend class ControlManager;

    void applyAbsoluteKnob(int knobIndex01, float normalized) noexcept;
    void setAssignButton(int assignIndex01, bool isDown) noexcept;
    void setChainPrevious(bool isDown) noexcept;
    void setChainNext(bool isDown) noexcept;
    void addEncoderDetents(int delta) noexcept;

    std::atomic<float> knobs[4] {};
    std::atomic<bool> assignButtons[2] {};
    std::atomic<bool> chainPrevDown { false };
    std::atomic<bool> chainNextDown { false };
    std::atomic<int> encoderDetentAccumulator { 0 };

    HardwareControlState(const HardwareControlState&) = delete;
    HardwareControlState& operator=(const HardwareControlState&) = delete;
};

} // namespace forge7
