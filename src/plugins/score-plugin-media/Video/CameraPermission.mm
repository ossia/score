#include "CameraPermission.hpp"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <memory>

namespace Video
{
CameraAuthorization cameraAuthorization() noexcept
{
  // Without a usage description in the bundle, macOS kills the process as soon as
  // it asks: keep the previous behaviour, where opening the device asks implicitly.
  if(![[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSCameraUsageDescription"])
    return CameraAuthorization::Authorized;

  switch([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo])
  {
    case AVAuthorizationStatusAuthorized:
      return CameraAuthorization::Authorized;
    case AVAuthorizationStatusNotDetermined:
      return CameraAuthorization::NotDetermined;
    case AVAuthorizationStatusDenied:
    case AVAuthorizationStatusRestricted:
    default:
      return CameraAuthorization::Denied;
  }
}

void requestCameraAuthorization(std::function<void(bool)> on_answer) noexcept
{
  auto callback = std::make_shared<std::function<void(bool)>>(std::move(on_answer));
  [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                           completionHandler:^(BOOL granted) {
                             // Called on an arbitrary queue
                             dispatch_async(dispatch_get_main_queue(), ^{
                               (*callback)(granted);
                             });
                           }];
}
}
