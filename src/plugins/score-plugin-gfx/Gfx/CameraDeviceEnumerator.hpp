#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <memory>
#include <vector>
namespace Gfx
{

struct CameraDeviceEnumerator
    : public std::enable_shared_from_this<CameraDeviceEnumerator>
{
  virtual ~CameraDeviceEnumerator() = default;

  virtual void registerAllEnumerators(Device::DeviceEnumerators& enums) = 0;
};

std::shared_ptr<CameraDeviceEnumerator> make_camera_enumerator();
}
