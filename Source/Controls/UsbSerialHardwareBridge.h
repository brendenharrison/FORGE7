#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

class ControlManager;

/** Placeholder for a microcontroller UART / USB-CDC path.

    For the product vocabulary “serial control input”, see `SerialControlInput` (thin facade over this class).

    Production flow: a background thread reads framed lines or binary packets from `juce::SerialPort`
    (or platform equivalent), parses into **the same** `HardwareControlEvent` shape, and calls
    `ControlManager::submitHardwareEvent` — **never** inject raw bytes into GUI.

    This TU stays a stub until the wire protocol is defined; keeps compile graph ready. */
class UsbSerialHardwareBridge
{
public:
    explicit UsbSerialHardwareBridge(ControlManager& controlManager);
    ~UsbSerialHardwareBridge();

    /** Future: open port at `devicePath` / baud; start read thread. */
    bool openConnection(const juce::String& devicePathHint);

    void closeConnection();

    /** Dev hook: feed one text line (`K1 0.5`, `ENC +1`, …) once protocol is specified. */
    void injectParsedLineForDevelopment(const juce::String& line);

private:
    ControlManager& controlManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UsbSerialHardwareBridge)
};

} // namespace forge7
