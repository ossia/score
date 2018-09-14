
#include "WiimoteProtocolSettingsWidget.hpp"

#include "WiimoteSpecificSettings.hpp"

#include <QLabel>
#include <QVariant>
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Engine::Network::WiimoteProtocolSettingsWidget)

namespace Engine::Network
{

WiimoteProtocolSettingsWidget::WiimoteProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("Wiimote");

  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(new QLabel(tr("Be sure to enable discoverable mode !")));

  setLayout(layout);
}

WiimoteProtocolSettingsWidget::~WiimoteProtocolSettingsWidget()
{
}

Device::DeviceSettings WiimoteProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  WiimoteSpecificSettings settings{};
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void WiimoteProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}
}
