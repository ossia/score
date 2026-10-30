#include "CameraPermission.hpp"

#if !defined(__APPLE__)
namespace Video
{
CameraAuthorization cameraAuthorization() noexcept
{
  return CameraAuthorization::Authorized;
}

void requestCameraAuthorization(std::function<void(bool)> on_answer) noexcept
{
  on_answer(true);
}
}
#endif
