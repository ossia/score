#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class PhidgetDevice final : public OwningOSSIADevice
{
  W_OBJECT(PhidgetDevice)
public:
  PhidgetDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
public:
  void sig_command()
  W_SIGNAL(sig_command);
private:
  void slot_command();
  W_SLOT(slot_command);

private:
  void timerEvent(QTimerEvent* event) override;
  int m_timer{-1};
};
}
}
