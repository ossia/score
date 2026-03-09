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
class QSplitter;
class QStackedWidget;
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
  void updatePropertiesPanel(int row);

  QLineEdit* m_deviceNameEdit{};
  QTableWidget* m_localPortsWidget{};
  QPushButton* m_localLoadLayout{};
  QPushButton* m_localAddPort{};
  QPushButton* m_localRmPort{};
  QLabel* m_boardLabel{};
  QComboBox* m_transport{};
  OSCTransportWidget* m_osc{};
  SimpleIOSpecificSettings m_impl;

  // Properties panel
  QSplitter* m_splitter{};
  QWidget* m_propertiesPanel{};
  QLineEdit* m_propName{};
  QLineEdit* m_propPath{};
  QComboBox* m_propType{};
  QStackedWidget* m_propStack{};
  QSpinBox* m_gpioLine{};
  QComboBox* m_gpioDirection{};
  QSpinBox* m_pwmChannel{};
  QSpinBox* m_adcChannel{};
  QSpinBox* m_dacChannel{};
  QSpinBox* m_neopixelPin{};
  QSpinBox* m_neopixelNumPixels{};
  int m_selectedPort{-1};
};
}
#endif
