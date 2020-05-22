// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQueryProtocolSettingsWidget.hpp"

#include "OSCQueryProtocolFactory.hpp"
#include "OSCQuerySpecificSettings.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Protocols/RateWidget.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QAction>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QNetworkReply>
#include <QPushButton>
#include <QString>
#include <QVariant>
#if defined(OSSIA_DNSSD)
#include <Explorer/Widgets/ZeroConf/ZeroconfBrowser.hpp>
#endif
namespace Protocols
{
OSCQueryProtocolSettingsWidget::OSCQueryProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  m_localHostEdit = new QLineEdit(this);
  m_rate = new RateWidget{this};

  QFormLayout* layout = new QFormLayout;

  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Host"), m_localHostEdit);
  layout->addRow(tr("Rate"), m_rate);

  setLayout(layout);

  setDefaults();
}

void OSCQueryProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_deviceNameEdit);
  SCORE_ASSERT(m_localHostEdit);

  m_deviceNameEdit->setText("newDevice");
  m_localHostEdit->setText("ws://127.0.0.1:5678");
  m_rate->setRate({});
}

Device::DeviceSettings OSCQueryProtocolSettingsWidget::getSettings() const
{
  SCORE_ASSERT(m_deviceNameEdit);

  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = OSCQueryProtocolFactory::static_concreteKey();

  OSCQuerySpecificSettings OSCQuery;
  OSCQuery.host = m_localHostEdit->text();

  OSCQuery.rate = m_rate->rate();

  s.deviceSpecificSettings = QVariant::fromValue(OSCQuery);
  return s;
}

void OSCQueryProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  OSCQuerySpecificSettings OSCQuery;
  if (settings.deviceSpecificSettings.canConvert<OSCQuerySpecificSettings>())
  {
    OSCQuery = settings.deviceSpecificSettings.value<OSCQuerySpecificSettings>();
    m_localHostEdit->setText(OSCQuery.host);
    m_rate->setRate(OSCQuery.rate);
  }
}
}
