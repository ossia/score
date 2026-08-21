#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/CAN/CANSpecificSettings.hpp>

#include <verdigris>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace Protocols
{

class CANProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(CANProtocolSettingsWidget)

public:
  explicit CANProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~CANProtocolSettingsWidget();

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void browseDBC();

  //! Parse the currently selected file and summarize it under the chooser, so
  //! that a wrong file or a wrong offset is visible before connecting.
  void updateDBCSummary();

  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_interface{};
  QLineEdit* m_dbcPath{};
  QSpinBox* m_nodeIdOffset{};
  QCheckBox* m_float32Override{};
  QCheckBox* m_fd{};
  QCheckBox* m_filterToDatabase{};
  QLabel* m_summary{};
};
}
#endif
