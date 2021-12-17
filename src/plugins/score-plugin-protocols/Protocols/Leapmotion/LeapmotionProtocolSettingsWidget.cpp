
#include "LeapmotionProtocolSettingsWidget.hpp"

#include "LeapmotionProtocolFactory.hpp"
#include "LeapmotionSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::LeapmotionProtocolSettingsWidget)

namespace Protocols
{

LeapmotionProtocolSettingsWidget::LeapmotionProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);
  m_deviceNameEdit->setText("Leapmotion");

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(new QLabel(tr("Be sure to enable discoverable mode !")));

  setLayout(layout);
}

LeapmotionProtocolSettingsWidget::~LeapmotionProtocolSettingsWidget() { }

Device::DeviceSettings LeapmotionProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = LeapmotionProtocolFactory::static_concreteKey();

  LeapmotionSpecificSettings settings{};
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void LeapmotionProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}

}
