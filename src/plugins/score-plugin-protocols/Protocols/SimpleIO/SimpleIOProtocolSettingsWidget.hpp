#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/SimpleIO/SimpleIOSpecificSettings.hpp>

#include <verdigris>

class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPushButton;

namespace Protocols
{

class SimpleIOProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(SimpleIOProtocolSettingsWidget)

public:
  SimpleIOProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~SimpleIOProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void updateTable();
  QLineEdit* m_deviceNameEdit{};
  QTableWidget* m_portsWidget{};
  QPushButton* m_addPort{};
  QPushButton* m_rmPort{};
  std::vector<SimpleIO::Port> m_ports;
};
}
#endif
