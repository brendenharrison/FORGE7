#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

class UsbSerialHardwareBridge;

/** Spec-facing name for MCU serial control: forwards to [UsbSerialHardwareBridge](UsbSerialHardwareBridge.h).

    Use this type at boundaries that should stay protocol-agnostic; the USB-CDC implementation
    remains in `UsbSerialHardwareBridge` until the newline protocol is finalized. */
class SerialControlInput
{
public:
    explicit SerialControlInput(UsbSerialHardwareBridge& bridge) noexcept
        : bridge(bridge)
    {
    }

    bool openConnection(const juce::String& devicePathHint) { return bridge.openConnection(devicePathHint); }

    void closeConnection() { bridge.closeConnection(); }

    void injectParsedLineForDevelopment(const juce::String& line) { bridge.injectParsedLineForDevelopment(line); }

private:
    UsbSerialHardwareBridge& bridge;
};

} // namespace forge7
