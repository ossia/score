
#include "ArtnetProtocolSettingsWidget.hpp"

#include "ArtnetSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Engine::Network::ArtnetProtocolSettingsWidget)

namespace Engine::Network
{

ArtnetProtocolSettingsWidget::ArtnetProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("Artnet");

  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);

  setLayout(layout);
}

ArtnetProtocolSettingsWidget::~ArtnetProtocolSettingsWidget()
{
}

Device::DeviceSettings ArtnetProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  ArtnetSpecificSettings settings{};
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void ArtnetProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}
}
