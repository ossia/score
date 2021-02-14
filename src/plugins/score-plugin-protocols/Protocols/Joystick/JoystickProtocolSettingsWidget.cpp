
#include "JoystickProtocolSettingsWidget.hpp"

#include "JoystickProtocolFactory.hpp"
#include "JoystickSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <ossia/protocols/joystick/joystick_protocol.hpp>
#include <score/widgets/ComboBox.hpp>

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

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);

  setLayout(layout);
}

JoystickProtocolSettingsWidget::~JoystickProtocolSettingsWidget() { }

Device::DeviceSettings JoystickProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = JoystickProtocolFactory::static_concreteKey();
  return s;
}

void JoystickProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);
}

}
