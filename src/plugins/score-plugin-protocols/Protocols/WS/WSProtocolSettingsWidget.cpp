// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "WSProtocolSettingsWidget.hpp"

#include "WSProtocolFactory.hpp"
#include "WSSpecificSettings.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Process/Script/ScriptWidget.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/tools/Debug.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCodeEditor>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QVariant>

namespace Protocols
{
WSProtocolSettingsWidget::WSProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  auto deviceNameLabel = new QLabel(tr("Name"), this);
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto addrLabel = new QLabel(tr("Address"), this);
  m_addressNameEdit = new QLineEdit{this};

  m_codeEdit = Process::createScriptWidget("JS");

  connect(m_codeEdit, &QCodeEditor::editingFinished, this, &WSProtocolSettingsWidget::parseHost);

  auto layout = new QGridLayout;

  layout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  layout->addWidget(m_deviceNameEdit, 0, 1, 1, 1);
  layout->addWidget(addrLabel, 1, 0, 1, 1);
  layout->addWidget(m_addressNameEdit, 1, 1, 1, 1);
  layout->addWidget(m_codeEdit, 3, 0, 1, 2);

  setLayout(layout);

  setDefaults();
}

void WSProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_deviceNameEdit);
  SCORE_ASSERT(m_codeEdit);

  m_deviceNameEdit->setText("newDevice");
  m_codeEdit->setPlainText("");
  m_addressNameEdit->clear();
}

void WSProtocolSettingsWidget::parseHost()
{
  auto engine = new QQmlEngine;
  auto comp = new QQmlComponent{engine};
  connect(comp, &QQmlComponent::statusChanged, this, [=](QQmlComponent::Status status) {
    switch (status)
    {
      case QQmlComponent::Status::Ready:
      {
        auto object = comp->create();
        if (auto prop = object->property("host").toString(); !prop.isEmpty())
        {
          m_addressNameEdit->setText(prop);
        }
        object->deleteLater();
        break;
      }
      default:
        qDebug() << status << comp->errorString();
    }
    comp->deleteLater();
    engine->deleteLater();
  });

  comp->setData(m_codeEdit->document()->toPlainText().toUtf8(), QUrl{});
}

Device::DeviceSettings WSProtocolSettingsWidget::getSettings() const
{
  SCORE_ASSERT(m_deviceNameEdit);

  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = WSProtocolFactory::static_concreteKey();

  WSSpecificSettings specific;
  specific.address = m_addressNameEdit->text();
  specific.text = m_codeEdit->toPlainText();

  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void WSProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  WSSpecificSettings specific;
  if (settings.deviceSpecificSettings.canConvert<WSSpecificSettings>())
  {
    specific = settings.deviceSpecificSettings.value<WSSpecificSettings>();

    m_addressNameEdit->setText(specific.address);
    if(specific.text != m_codeEdit->toPlainText())
    {
      m_codeEdit->setPlainText(specific.text);
      parseHost();
    }
  }
}
}
