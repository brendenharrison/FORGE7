#include "MacAudioPermission.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Utilities/Logger.h"

#if JUCE_MAC
#import <AVFoundation/AVFoundation.h>

namespace forge7
{
namespace
{

juce::String avAuthorizationStatusString(AVAuthorizationStatus s)
{
    switch (s)
    {
        case AVAuthorizationStatusNotDetermined:
            return "NotDetermined";
        case AVAuthorizationStatusRestricted:
            return "Restricted";
        case AVAuthorizationStatusDenied:
            return "Denied";
        case AVAuthorizationStatusAuthorized:
            return "Authorized";
        default:
            return "Unknown(" + juce::String((int) s) + ")";
    }
}

} // namespace

MacMicPermissionStatus getMacMicPermissionStatus()
{
    const AVAuthorizationStatus av = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

    static AVAuthorizationStatus lastLogged { (AVAuthorizationStatus) - 1 };
    if (av != lastLogged)
    {
        lastLogged = av;
        Logger::info("MacAudioPermission: getMacMicPermissionStatus (JUCE_MAC .mm) AVAuthorizationStatus="
                     + avAuthorizationStatusString(av));
    }

    switch (av)
    {
        case AVAuthorizationStatusNotDetermined:
            return MacMicPermissionStatus::NotDetermined;
        case AVAuthorizationStatusDenied:
            return MacMicPermissionStatus::Denied;
        case AVAuthorizationStatusRestricted:
            return MacMicPermissionStatus::Restricted;
        case AVAuthorizationStatusAuthorized:
            return MacMicPermissionStatus::Authorized;
        default:
            return MacMicPermissionStatus::Unknown;
    }
}

void requestMacMicPermission(std::function<void(bool granted)> callback)
{
    const AVAuthorizationStatus before = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    Logger::info("MacAudioPermission: requestMacMicPermission entry (JUCE_MAC .mm), current AVAuthorizationStatus="
                 + avAuthorizationStatusString(before));

    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL granted)
                             {
                                 juce::MessageManager::callAsync([callback = std::move(callback), granted]() mutable
                                                                  {
                                                                      Logger::info(
                                                                          "MacAudioPermission: requestAccess "
                                                                          "completion handler ran on message thread, "
                                                                          "granted="
                                                                          + juce::String(granted == YES ? "true" : "false"));

                                                                      if (callback != nullptr)
                                                                          callback(granted == YES);
                                                                  });
                             }];
}

} // namespace forge7

#else

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

#endif
