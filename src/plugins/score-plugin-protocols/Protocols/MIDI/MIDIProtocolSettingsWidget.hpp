#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/midi/midi.hpp>

#include <verdigris>

namespace score { class ComboBox;}
class QCheckBox;
class QRadioButton;
class QWidget;
class QLineEdit;

namespace Protocols
{
class MIDIProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(MIDIProtocolSettingsWidget)

public:
  MIDIProtocolSettingsWidget(QWidget* parent = nullptr);

private:
  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

  void updateDevices(ossia::net::midi::midi_info::Type);
  void updateInputDevices();
  void updateOutputDevices();

  QLineEdit* m_name{};
  QRadioButton* m_inButton{};
  QRadioButton* m_outButton{};
  score::ComboBox* m_deviceCBox{};
  QCheckBox* m_createWhole{};
};
}
