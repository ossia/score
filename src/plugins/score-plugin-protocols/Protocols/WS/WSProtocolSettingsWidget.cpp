// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "WSProtocolSettingsWidget.hpp"

#include "WSProtocolFactory.hpp"
#include "WSSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Process/Script/ScriptWidget.hpp>

#include <score/tools/Debug.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCodeEditor>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSplitter>
#include <QVariant>

namespace Protocols
{
WSProtocolSettingsWidget::WSProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  auto deviceNameLabel = new QLabel(tr("Name"), this);
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto addrLabel = new QLabel(tr("Address"), this);
  m_addressNameEdit = new QLineEdit{this};

  m_codeEdit = Process::createScriptWidget("JS");
  checkForChanges(m_codeEdit);

  m_errorPane = new QPlainTextEdit{this};
  m_errorPane->setReadOnly(true);
  m_errorPane->setMaximumHeight(120);

  m_splitter = new QSplitter{Qt::Vertical, this};
  m_splitter->addWidget(m_codeEdit);
  m_splitter->addWidget(m_errorPane);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);

  auto validateBtn = new QPushButton{tr("Validate"), this};
  connect(validateBtn, &QPushButton::clicked, this, &WSProtocolSettingsWidget::validate);

  connect(
      static_cast<QCodeEditor*>(m_codeEdit), &QCodeEditor::editingFinished, this,
      &WSProtocolSettingsWidget::parseHost);

  auto layout = new QGridLayout;

  layout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  layout->addWidget(m_deviceNameEdit, 0, 1, 1, 1);
  layout->addWidget(addrLabel, 1, 0, 1, 1);
  layout->addWidget(m_addressNameEdit, 1, 1, 1, 1);
  layout->addWidget(m_splitter, 3, 0, 1, 2);
  layout->addWidget(validateBtn, 4, 0, 1, 2);

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
  connect(
      comp, &QQmlComponent::statusChanged, this,
      [this, comp, engine](QQmlComponent::Status status) {
    switch(status)
    {
      case QQmlComponent::Status::Ready: {
        auto object = comp->create();
        if(auto prop = object->property("host").toString(); !prop.isEmpty())
        {
          m_addressNameEdit->setText(prop);
        }
        object->deleteLater();
        break;
      }
      case QQmlComponent::Status::Error: {
        auto errors = comp->errorString();
        if(errors.startsWith(':'))
          errors = QStringLiteral("L") + errors.mid(1);
        errors.replace(QStringLiteral("\n:"), QStringLiteral("\nL"));
        qDebug() << "WebSocket:" << errors;
        m_errorPane->setPlainText(errors);
      } break;
      default:
        break;
    }
    comp->deleteLater();
    engine->deleteLater();
      });

  comp->setData(m_codeEdit->document()->toPlainText().toUtf8(), QUrl{});
}

void WSProtocolSettingsWidget::validate()
{
  m_errorPane->clear();

  auto code = m_codeEdit->toPlainText().toUtf8();
  if(code.isEmpty())
    return;

  auto engine = new QQmlEngine{this};
  auto comp = new QQmlComponent{engine};

  connect(
      comp, &QQmlComponent::statusChanged, this,
      [this, comp, engine](QQmlComponent::Status status) {
    switch(status)
    {
      case QQmlComponent::Status::Ready:
        m_errorPane->setPlainText(tr("QML code is valid."));
        break;
      case QQmlComponent::Status::Error: {
        auto errors = comp->errorString();
        if(errors.startsWith(':'))
          errors = QStringLiteral("L") + errors.mid(1);
        errors.replace(QStringLiteral("\n:"), QStringLiteral("\nL"));
        qDebug() << "WebSocket:" << errors;
        m_errorPane->setPlainText(errors);
      } break;
      default:
        break;
    }
    comp->deleteLater();
    engine->deleteLater();
  });

  comp->setData(code, QUrl{});
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
  if(settings.deviceSpecificSettings.canConvert<WSSpecificSettings>())
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
