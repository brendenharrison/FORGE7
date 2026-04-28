#pragma once

#include <functional>
#include <memory>

#include <juce_core/juce_core.h>

namespace forge7
{

class ControlManager;

/** Narrow OS/HAL boundary for LattePanda Mu Linux: GPIO expander, /dev/input, rotary
    encoder quadrature, SPI displays, etc. Implementations live under Drivers/ platform
    folders later. ControlManager polls or subscribes via this interface - no knowledge
    of sysfs paths here.

    Keep polling on a non-audio thread; signal ControlManager through thread-safe queues. */
class HardwareAbstraction
{
public:
    virtual ~HardwareAbstraction() = default;

    /** Optional: periodic poll for knob/button deltas; return true if state changed. */
    virtual bool pollHardwareState() = 0;

    /** Optional callback registration for edge-triggered encoder steps. */
    virtual void setEncoderDeltaCallback(std::function<void(int deltaDetents)> callback) = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwareAbstraction)
};

/** Placeholder no-op implementation for desktop bring-up until real drivers exist. */
class NullHardwareAbstraction final : public HardwareAbstraction
{
public:
    NullHardwareAbstraction() = default;

    bool pollHardwareState() override { return false; }

    void setEncoderDeltaCallback(std::function<void(int)> callback) override { juce::ignoreUnused(callback); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NullHardwareAbstraction)
};

} // namespace forge7
