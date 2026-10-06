
#include "JoystickProtocolSettingsWidget.hpp"

#include "JoystickProtocolFactory.hpp"
#include "JoystickSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/widgets/ComboBox.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <ossia/protocols/joystick/joystick_protocol.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
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
  checkForChanges(m_deviceNameEdit);
  m_deviceNameEdit->setText("Joystick");

  m_gamepad = new QCheckBox{this};
  m_gamepad->setChecked(false);
  score::setHelp(
      m_gamepad, tr("Use the SDL gamepad API, which also exposes rumble and motion sensors"));

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Gamepad API"), m_gamepad);

#if defined(__EMSCRIPTEN__)
  auto hint = new QLabel{
      tr("Press a button or move an axis to make the joystick visible to the "
         "browser."),
      this};
  hint->setWordWrap(true);
  layout->addRow(hint);
#endif

  setLayout(layout);
}

JoystickProtocolSettingsWidget::~JoystickProtocolSettingsWidget() { }

Device::DeviceSettings JoystickProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = JoystickProtocolFactory::static_concreteKey();
  if(s.deviceSpecificSettings.canConvert<JoystickSpecificSettings>())
  {
    auto specif = s.deviceSpecificSettings.value<JoystickSpecificSettings>();
    specif.gamepad = m_gamepad->isChecked();
    s.deviceSpecificSettings = QVariant::fromValue(specif);
  }
  return s;
}

void JoystickProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);
  if(m_settings.deviceSpecificSettings.canConvert<JoystickSpecificSettings>())
  {
    auto specif = m_settings.deviceSpecificSettings.value<JoystickSpecificSettings>();
    m_gamepad->setChecked(specif.gamepad);
  }
}

}
