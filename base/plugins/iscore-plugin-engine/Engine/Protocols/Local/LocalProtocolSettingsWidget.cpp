#include "LocalProtocolSettingsWidget.hpp"
#include "LocalProtocolFactory.hpp"
#include "LocalSpecificSettings.hpp"
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <iscore/widgets/TextLabel.hpp>

class QWidget;

namespace Engine
{
namespace Network
{
LocalProtocolSettingsWidget::LocalProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  auto lay = new QFormLayout;
  QLabel* deviceNameLabel = new TextLabel(tr("Local device"), this);
  lay->addWidget(deviceNameLabel);
  m_remoteNameEdit = new QLineEdit;
  m_localHostEdit = new QLineEdit;
  m_localPort = new QSpinBox;
  m_remotePort = new QSpinBox;
  m_localPort->setRange(0, 65535);
  m_remotePort->setRange(0, 65535);
  lay->addRow(tr("Remote name"), m_remoteNameEdit);
  lay->addRow(tr("Host"), m_localHostEdit);
  lay->addRow(tr("Local port"), m_localPort);
  lay->addRow(tr("Remote port"), m_remotePort);

  setLayout(lay);

  setSettings(LocalProtocolFactory::static_defaultSettings());
}

Device::DeviceSettings LocalProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = LocalProtocolFactory::static_defaultSettings();
  // TODO *** protocol was never being set here. Check everywhere.! ***
  s.name = "i-score";
  Network::LocalSpecificSettings local;
  local.host = m_localHostEdit->text();
  local.remoteName = m_remoteNameEdit->text();
  local.localPort = m_localPort->value();
  local.remotePort = m_remotePort->value();
  s.deviceSpecificSettings = QVariant::fromValue(local);
  return s;
}

void LocalProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  if (settings.deviceSpecificSettings
          .canConvert<Network::LocalSpecificSettings>())
  {
    auto set = settings.deviceSpecificSettings
                   .value<Network::LocalSpecificSettings>();
    m_remoteNameEdit->setText(set.remoteName);
    m_localHostEdit->setText(set.host);
    m_localPort->setValue(set.localPort);
    m_remotePort->setValue(set.remotePort);
  }
}
}
}
