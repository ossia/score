#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class PhidgetDevice final : public OwningOSSIADevice
{
    Q_OBJECT
public:
  PhidgetDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
  signals:
    void sig_command();
  private slots:
    void slot_command();

};
}
}
