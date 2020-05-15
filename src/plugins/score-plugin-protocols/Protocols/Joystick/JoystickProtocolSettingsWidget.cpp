
#include "JoystickProtocolSettingsWidget.hpp"

#include "JoystickSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <ossia/network/joystick/joystick_protocol.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::JoystickProtocolSettingsWidget)

namespace Protocols
{

JoystickProtocolSettingsWidget::JoystickProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("Joystick");

  m_deviceChoice = new QComboBox{this};

  auto update_button = new QPushButton{"Update", this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("Joystick"), m_deviceChoice);
  layout->addRow(update_button);

  setLayout(layout);

  connect(
      update_button,
      &QAbstractButton::released,
      this,
      &JoystickProtocolSettingsWidget::update_device_list);

  update_device_list();
}

JoystickProtocolSettingsWidget::~JoystickProtocolSettingsWidget() {}

Device::DeviceSettings JoystickProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  const int index = m_deviceChoice->currentIndex();
  const int32_t id = ossia::net::joystick_protocol::get_joystick_id(index);

  JoystickSpecificSettings settings{id, index};
  s.deviceSpecificSettings = QVariant::fromValue(settings);
  return s;
}

void JoystickProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}

void JoystickProtocolSettingsWidget::update_device_list()
{
  m_deviceChoice->clear();

  const unsigned int joystick_count
      = ossia::net::joystick_protocol::get_joystick_count();

  for (unsigned int i = 0; i < joystick_count; ++i)
  {
    const char* s = ossia::net::joystick_protocol::get_joystick_name(i);
    m_deviceChoice->addItem(s);
  }
}
}
