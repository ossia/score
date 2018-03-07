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
  Q_SIGNALS:
    void sig_command();
  private Q_SLOTS:
    void slot_command();

  private:
    void timerEvent(QTimerEvent* event) override;
    int m_timer{-1};
};
}
}
