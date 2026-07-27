#pragma once

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSplitter;
class QTextEdit;
class QTimer;
class QWidget;
namespace score
{
class ComboBox;
}

namespace Protocols
{
class SerialProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  SerialProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();
  void validate();
  void reloadPorts();

protected:
  QLineEdit* m_name{};
  score::ComboBox* m_port{};
  score::ComboBox* m_rate{};
  QSplitter* m_splitter{};
  QTextEdit* m_codeEdit{};
  QPlainTextEdit* m_errorPane{};

#if defined(__EMSCRIPTEN__)
  void pollBrowserPorts();

  QLabel* m_permissionHint{};
  QTimer* m_pollTimer{};
  int m_generation{-1};
#endif
};
}

#endif
