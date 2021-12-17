#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QLineEdit;

namespace Protocols
{

class LeapmotionProtocolSettingsWidget final
    : public Device::ProtocolSettingsWidget
{
  W_OBJECT(LeapmotionProtocolSettingsWidget)

public:
  LeapmotionProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~LeapmotionProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
};

}
