#include "MacAudioPermission.h"

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_MAC
#import <AVFoundation/AVFoundation.h>

namespace forge7
{

MacMicPermissionStatus getMacMicPermissionStatus()
{
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
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
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL granted)
                             {
                                 juce::MessageManager::callAsync([callback = std::move(callback), granted]() mutable
                                                                  {
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
