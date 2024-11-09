#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <QNetworkAccessManager>

class QLineEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{
class RateWidget;
class OSCQueryProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  OSCQueryProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_localHostEdit{};
  QSpinBox* m_localPort{};
  QCheckBox* m_dense{};

  RateWidget* m_rate{};
};
}
