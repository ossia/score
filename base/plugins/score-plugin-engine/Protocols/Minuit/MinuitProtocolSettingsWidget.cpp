// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MinuitProtocolSettingsWidget.hpp"

#include "MinuitSpecificSettings.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QAction>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVariant>

#if defined(OSSIA_DNSSD)
#include <Explorer/Widgets/ZeroConf/ZeroconfBrowser.hpp>
#endif

namespace Protocols
{
MinuitProtocolSettingsWidget::MinuitProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  m_portInputSBox = new QSpinBox(this);
  m_portInputSBox->setRange(0, 65535);

  m_portOutputSBox = new QSpinBox(this);
  m_portOutputSBox->setRange(0, 65535);

  m_localHostEdit = new QLineEdit(this);
  m_localNameEdit = new QLineEdit(this);

  QFormLayout* layout = new QFormLayout;

#if defined(OSSIA_DNSSD)
  m_browser = new ZeroconfBrowser{"_minuit._tcp", this};
  auto pb = new QPushButton{tr("Find devices..."), this};
  connect(
      pb, &QPushButton::clicked, m_browser->makeAction(), &QAction::trigger);
  connect(
      m_browser, &ZeroconfBrowser::sessionSelected, this,
      [=](QString name, QString ip, int port, QMap<QString, QByteArray> txt) {
        m_deviceNameEdit->setText(name);
        m_portInputSBox->setValue(port);
        m_portOutputSBox->setValue(txt["RemotePort"].toInt());
        m_localHostEdit->setText(ip);
      });
  layout->addWidget(pb);
#endif

  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Device listening port"), m_portInputSBox);
  layout->addRow(tr("score listening port"), m_portOutputSBox);
  layout->addRow(tr("Host"), m_localHostEdit);
  layout->addRow(tr("Local Name"), m_localNameEdit);

  setLayout(layout);

  setDefaults();
}

void MinuitProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_deviceNameEdit);
  SCORE_ASSERT(m_portOutputSBox);
  SCORE_ASSERT(m_localHostEdit);

  m_deviceNameEdit->setText("newDevice");
  m_portInputSBox->setValue(9998);
  m_portOutputSBox->setValue(13579);
  m_localHostEdit->setText("127.0.0.1");
  m_localNameEdit->setText("score");
}

Device::DeviceSettings MinuitProtocolSettingsWidget::getSettings() const
{
  SCORE_ASSERT(m_deviceNameEdit);

  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  MinuitSpecificSettings minuit;
  minuit.host = m_localHostEdit->text();
  minuit.localName = m_localNameEdit->text();
  minuit.inputPort = m_portInputSBox->value();
  minuit.outputPort = m_portOutputSBox->value();

  s.deviceSpecificSettings = QVariant::fromValue(minuit);
  return s;
}

void MinuitProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  MinuitSpecificSettings minuit;
  if (settings.deviceSpecificSettings
          .canConvert<MinuitSpecificSettings>())
  {
    minuit = settings.deviceSpecificSettings
                 .value<MinuitSpecificSettings>();
    m_portInputSBox->setValue(minuit.inputPort);
    m_portOutputSBox->setValue(minuit.outputPort);
    m_localHostEdit->setText(minuit.host);
    m_localNameEdit->setText(minuit.localName);
  }
}
}
