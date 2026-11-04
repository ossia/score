#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QLineEdit;
class QComboBox;
class QLabel;

namespace Protocols
{
class HIDProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(HIDProtocolSettingsWidget)

public:
  HIDProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~HIDProtocolSettingsWidget();

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void rescan();
  void updateDetails();

  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_devices{};
  QLabel* m_details{};
  Device::DeviceSettings m_settings;
};
}
