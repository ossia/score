#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/SimpleIO/SimpleIOSpecificSettings.hpp>

#include <verdigris>

class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QPushButton;

namespace Protocols
{
class OSCTransportWidget;
class SimpleIOProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(SimpleIOProtocolSettingsWidget)

public:
  SimpleIOProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~SimpleIOProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void updateGui();
  void updateTable();

  QLineEdit* m_deviceNameEdit{};
  QTableWidget* m_localPortsWidget{};
  QPushButton* m_localLoadLayout{};
  QPushButton* m_localAddPort{};
  QPushButton* m_localRmPort{};
  QLabel* m_boardLabel{};
  QComboBox* m_transport{};
  OSCTransportWidget* m_osc{};
  SimpleIOSpecificSettings m_impl;
};
}
#endif
