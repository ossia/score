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
class WSProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  WSProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();
  void parseHost();
  void validate();

protected:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_addressNameEdit{};
  QSplitter* m_splitter{};
  QTextEdit* m_codeEdit{};
  QPlainTextEdit* m_errorPane{};
};
}
