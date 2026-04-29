#include "MacAudioPermission.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

MacMicPermissionStatus getMacMicPermissionStatus()
{
    return MacMicPermissionStatus::NotMac;
}

void requestMacMicPermission(std::function<void(bool granted)> callback)
{
    juce::MessageManager::callAsync([cb = std::move(callback)]() mutable
                                    {
                                        if (cb != nullptr)
                                            cb(false);
                                    });
}

} // namespace forge7
