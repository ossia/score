// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QSpinBox>
#include <QString>
#include <QVariant>

#include "PhidgetsProtocolSettingsWidget.hpp"
#include "PhidgetsSpecificSettings.hpp"
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <QFormLayout>
#include <QComboBox>
#include <QPlainTextEdit>
#include <score/widgets/JS/JSEdit.hpp>
class QWidget;

namespace Engine
{
namespace Network
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
  Network::PhidgetSpecificSettings specific;
  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void PhidgetProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
}
}
}
