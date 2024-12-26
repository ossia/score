#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/protocols/midi/midi.hpp>

#include <verdigris>

namespace libremidi
{
class observer;
struct input_port;
struct output_port;
}
namespace State
{
class AddressFragmentLineEdit;
}
namespace score
{
class ComboBox;
}
class QCheckBox;
class QRadioButton;
class QWidget;
class QLineEdit;

namespace Protocols
{
class MCUSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(MCUSettingsWidget)

public:
  explicit MCUSettingsWidget(QWidget* parent = nullptr);
  ~MCUSettingsWidget();

private:
  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

  State::AddressFragmentLineEdit* m_name{};
  QComboBox* m_midiin{};
  QComboBox* m_midiout{};
  Device::DeviceSettings m_current;

  std::unique_ptr<libremidi::observer> m_observer{};

  std::vector<libremidi::input_port> m_ins;
  std::vector<libremidi::output_port> m_outs;
};
}
