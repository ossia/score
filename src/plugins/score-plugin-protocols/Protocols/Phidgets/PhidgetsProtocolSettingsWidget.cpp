// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_PHIDGETS)
#include "PhidgetsProtocolSettingsWidget.hpp"
#include "PhidgetsSpecificSettings.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/widgets/JS/JSEdit.hpp>

#include <QFormLayout>
#include <QLineEdit>
#include <QVariant>
class QWidget;

namespace Protocols
{
PhidgetProtocolSettingsWidget::PhidgetProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new QLineEdit;
  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);

  setLayout(lay);

  setDefaults();
}

void PhidgetProtocolSettingsWidget::setDefaults()
{
  m_name->setText("Phidgets");
}

Device::DeviceSettings PhidgetProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_name->text();
  PhidgetSpecificSettings specific;
  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void PhidgetProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
}
}
#endif
