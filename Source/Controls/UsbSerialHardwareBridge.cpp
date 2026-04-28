#include "UsbSerialHardwareBridge.h"

#include "ControlManager.h"

#include "../Utilities/Logger.h"

namespace forge7
{

UsbSerialHardwareBridge::UsbSerialHardwareBridge(ControlManager& cm)
    : controlManager(cm)
{
}

UsbSerialHardwareBridge::~UsbSerialHardwareBridge() = default;

bool UsbSerialHardwareBridge::openConnection(const juce::String& devicePathHint)
{
    juce::ignoreUnused(devicePathHint);
    Logger::info("FORGE7: UsbSerialHardwareBridge::openConnection — stub (no wire protocol yet)");
    return false;
}

void UsbSerialHardwareBridge::closeConnection() {}

void UsbSerialHardwareBridge::injectParsedLineForDevelopment(const juce::String& line)
{
    juce::ignoreUnused(line);
    juce::ignoreUnused(controlManager);
    Logger::info("FORGE7: UsbSerialHardwareBridge::injectParsedLineForDevelopment — stub");
}

} // namespace forge7
