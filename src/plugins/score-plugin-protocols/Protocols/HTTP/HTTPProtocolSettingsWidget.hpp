#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
class QLineEdit;
class QPlainTextEdit;
class QSplitter;
class QTextEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{
class HTTPProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  HTTPProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();
  void validate();

protected:
  QLineEdit* m_deviceNameEdit{};
  QSplitter* m_splitter{};
  QTextEdit* m_codeEdit{};
  QPlainTextEdit* m_errorPane{};
};
}
