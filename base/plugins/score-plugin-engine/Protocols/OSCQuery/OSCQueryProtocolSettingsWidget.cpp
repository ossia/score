// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQueryProtocolSettingsWidget.hpp"

#include "OSCQuerySpecificSettings.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QAction>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVariant>
#if defined(OSSIA_DNSSD)
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

  connect(
      &m_http_client, &QNetworkAccessManager::finished, this,
      [&](QNetworkReply* ret) {
        if (ret != m_cur_reply)
        {
          ret->deleteLater();
          return;
        }

        auto doc = QJsonDocument::fromJson(ret->readAll());
        if (doc.object().contains("NAME"))
        {
          auto str = doc.object()["NAME"].toString();
          if (!str.isEmpty())
            m_deviceNameEdit->setText(str);
        }
        ret->deleteLater();
        m_cur_reply = nullptr;
      });

  QFormLayout* layout = new QFormLayout;

#if defined(OSSIA_DNSSD)
  m_browser = new ZeroconfBrowser{"_oscjson._tcp", this};
  auto pb = new QPushButton{tr("Find devices..."), this};
  connect(
      pb, &QPushButton::clicked, m_browser->makeAction(), &QAction::trigger);
  connect(
      m_browser, &ZeroconfBrowser::sessionSelected, this,
      [=](QString name, QString ip, int port, QMap<QString, QByteArray> txt) {
        m_deviceNameEdit->setText(name);

        if (auto ret = m_http_client.get(QNetworkRequest(QUrl(
                "http://" + ip + ":" + QString::number(port)
                + "/?HOST_INFO"))))
        {
          m_cur_reply = ret;
        }

        if (txt.contains("WebSockets") && txt["WebSockets"] == "true")
          m_localHostEdit->setText("ws://" + ip + ":" + QString::number(port));
        else
          m_localHostEdit->setText(
              "http://" + ip + ":" + QString::number(port));
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
  SCORE_ASSERT(m_deviceNameEdit);
  SCORE_ASSERT(m_localHostEdit);

  m_deviceNameEdit->setText("newDevice");
  m_localHostEdit->setText("ws://127.0.0.1:5678");
}

Device::DeviceSettings OSCQueryProtocolSettingsWidget::getSettings() const
{
  SCORE_ASSERT(m_deviceNameEdit);

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
