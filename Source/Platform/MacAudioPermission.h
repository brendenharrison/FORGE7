#pragma once

#include <functional>

namespace forge7
{

enum class MacMicPermissionStatus
{
    NotMac,
    Unknown,
    NotDetermined,
    Denied,
    Restricted,
    Authorized
};

MacMicPermissionStatus getMacMicPermissionStatus();

/** Requests microphone capture permission (macOS). Callback is always invoked on the JUCE message thread. */
void requestMacMicPermission(std::function<void(bool granted)> callback);

} // namespace forge7
