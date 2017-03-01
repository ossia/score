#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include "OSCQueryProtocolSettingsWidget.hpp"
#include "OSCQuerySpecificSettings.hpp"
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <QAction>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVariant>

#if defined(ISCORE_ZEROCONF)
#include <Explorer/Widgets/ZeroConf/ZeroconfBrowser.hpp>
#endif

class QWidget;

namespace Engine
{
namespace Network
{
OSCQueryProtocolSettingsWidget::OSCQueryProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  m_localHostEdit = new QLineEdit(this);

  QFormLayout* layout = new QFormLayout;

#if defined(ISCORE_ZEROCONF)
  m_browser = new ZeroconfBrowser{"_oscjson._tcp", this};
  auto pb = new QPushButton{tr("Find devices..."), this};
  connect(
      pb, &QPushButton::clicked, m_browser->makeAction(), &QAction::trigger);
  connect(
      m_browser, &ZeroconfBrowser::sessionSelected, this,
      [=](QString name, QString ip, int port, QMap<QString, QByteArray> txt) {
        m_deviceNameEdit->setText(name);
        m_localHostEdit->setText("ws://" + ip + ":" + QString::number(port));
      });
  layout->addWidget(pb);
#endif

  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Host"), m_localHostEdit);

  setLayout(layout);

  setDefaults();
}

void OSCQueryProtocolSettingsWidget::setDefaults()
{
  ISCORE_ASSERT(m_deviceNameEdit);
  ISCORE_ASSERT(m_localHostEdit);

  m_deviceNameEdit->setText("newDevice");
  m_localHostEdit->setText("ws://127.0.0.1:5678");
}

Device::DeviceSettings OSCQueryProtocolSettingsWidget::getSettings() const
{
  ISCORE_ASSERT(m_deviceNameEdit);

  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  Network::OSCQuerySpecificSettings OSCQuery;
  OSCQuery.host = m_localHostEdit->text();

  s.deviceSpecificSettings = QVariant::fromValue(OSCQuery);
  return s;
}

void OSCQueryProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  Network::OSCQuerySpecificSettings OSCQuery;
  if (settings.deviceSpecificSettings
          .canConvert<Network::OSCQuerySpecificSettings>())
  {
    OSCQuery = settings.deviceSpecificSettings
                 .value<Network::OSCQuerySpecificSettings>();
    m_localHostEdit->setText(OSCQuery.host);
  }
}
}
}
