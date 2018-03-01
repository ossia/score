#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QLineEdit;
class QSpinBox;
class QWidget;

namespace Engine
{
namespace Network
{
class OSCProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  Q_OBJECT

public:
  OSCProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  QString getPath() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected Q_SLOTS:
  void openFileDialog();

protected:
  void setDefaults();

protected:
  QLineEdit* m_deviceNameEdit;
  QSpinBox* m_portOutputSBox;
  QSpinBox* m_portInputSBox;
  QLineEdit* m_localHostEdit;
  QLineEdit* m_namespaceFilePathEdit;
};
}
}
