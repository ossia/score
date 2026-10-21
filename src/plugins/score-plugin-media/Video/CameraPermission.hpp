#pragma once
#include <score_plugin_media_export.h>

#include <functional>

namespace Video
{
enum class CameraAuthorization
{
  Authorized,
  Denied,
  NotDetermined
};

//! Whether the OS lets this process capture from cameras.
//! Always Authorized where the OS has no such permission.
SCORE_PLUGIN_MEDIA_EXPORT
CameraAuthorization cameraAuthorization() noexcept;

//! Shows the OS prompt asking the user for camera access.
//! on_answer runs on the main thread once the user has answered.
SCORE_PLUGIN_MEDIA_EXPORT
void requestCameraAuthorization(std::function<void(bool granted)> on_answer) noexcept;
}
