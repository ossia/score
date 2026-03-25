// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "HTTPProtocolSettingsWidget.hpp"

#include "HTTPProtocolFactory.hpp"
#include "HTTPSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Process/Script/ScriptWidget.hpp>

#include <score/widgets/TextLabel.hpp>

#include <QCodeEditor>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSplitter>
#include <QVariant>
class QWidget;

namespace Protocols
{
HTTPProtocolSettingsWidget::HTTPProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  QLabel* deviceNameLabel = new TextLabel(tr("Name"), this);
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

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
  connect(validateBtn, &QPushButton::clicked, this, &HTTPProtocolSettingsWidget::validate);

  QGridLayout* gLayout = new QGridLayout;

  gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  gLayout->addWidget(m_deviceNameEdit, 0, 1, 1, 1);
  gLayout->addWidget(m_splitter, 3, 0, 1, 2);
  gLayout->addWidget(validateBtn, 4, 0, 1, 2);

  setLayout(gLayout);

  setDefaults();
}

void HTTPProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_deviceNameEdit);
  SCORE_ASSERT(m_codeEdit);

  m_deviceNameEdit->setText("newDevice");
  m_codeEdit->setPlainText("");
}

void HTTPProtocolSettingsWidget::validate()
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
        qDebug() << "HTTP:" << errors;
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

Device::DeviceSettings HTTPProtocolSettingsWidget::getSettings() const
{
  SCORE_ASSERT(m_deviceNameEdit);

  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = HTTPProtocolFactory::static_concreteKey();

  HTTPSpecificSettings specific;
  specific.text = m_codeEdit->toPlainText();

  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void HTTPProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  HTTPSpecificSettings specific;
  if(settings.deviceSpecificSettings.canConvert<HTTPSpecificSettings>())
  {
    specific = settings.deviceSpecificSettings.value<HTTPSpecificSettings>();
    m_codeEdit->setPlainText(specific.text);
  }
}
}
