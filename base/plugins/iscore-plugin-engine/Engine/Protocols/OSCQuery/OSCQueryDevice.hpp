#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

Q_DECLARE_METATYPE(std::function<void()>)
namespace Engine
{
namespace Network
{
class OSCQueryDevice final : public OwningOSSIADevice
{
  Q_OBJECT
public:
  OSCQueryDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;
  void recreate(const Device::Node& n) override;

signals:
  void sig_command();
  void sig_disconnect();
private slots:
  void slot_command();
};
}
}
