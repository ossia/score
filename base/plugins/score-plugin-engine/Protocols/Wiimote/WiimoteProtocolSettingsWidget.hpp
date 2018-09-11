#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <wobjectdefs.h>

class QLineEdit;
class QComboBox;

namespace Engine::Network {

class WiimoteProtocolSettingsWidget final
    : public Device::ProtocolSettingsWidget
{
  W_OBJECT(WiimoteProtocolSettingsWidget)

public:
  WiimoteProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~WiimoteProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

public:
  void update_device_list();

protected:
  QLineEdit* m_deviceNameEdit{};
};

}
