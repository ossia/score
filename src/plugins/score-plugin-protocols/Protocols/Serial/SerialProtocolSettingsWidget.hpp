#pragma once

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
class QLineEdit;
class QCodeEditor;
class QWidget;
namespace score { class ComboBox;}

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

protected:
  QLineEdit* m_name{};
  score::ComboBox* m_port{};
  QCodeEditor* m_codeEdit{};
};
}

#endif
