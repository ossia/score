// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include "SerialProtocolFactory.hpp"
#include "SerialProtocolSettingsWidget.hpp"
#include "SerialSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Process/Script/ScriptWidget.hpp>

#include <score/widgets/ComboBox.hpp>
#include <score/widgets/Layout.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCodeEditor>
#include <QFileInfo>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSplitter>
#include <QTimer>
#include <QVariant>

#if defined(__EMSCRIPTEN__)
#include <Protocols/Serial/WebSerial.hpp>
#endif

namespace Protocols
{
SerialProtocolSettingsWidget::SerialProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  QLabel* deviceNameLabel = new TextLabel(tr("Name"), this);
  m_name = new QLineEdit{this};
  checkForChanges(m_name);

  QLabel* portLabel = new TextLabel(tr("Port"), this);
  m_port = new score::ComboBox{this};

  QLabel* rateLabel = new TextLabel(tr("Baud rate"), this);
  m_rate = new score::ComboBox{this};
  m_rate->setEditable(true);

  m_codeEdit = Process::createScriptWidget("JS");
  m_port->setEditable(true);

  m_errorPane = new QPlainTextEdit{this};
  m_errorPane->setReadOnly(true);
  m_errorPane->setMaximumHeight(120);

  m_splitter = new QSplitter{Qt::Vertical, this};
  m_splitter->addWidget(m_codeEdit);
  m_splitter->addWidget(m_errorPane);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);

  auto validateBtn = new QPushButton{tr("Validate"), this};
  connect(validateBtn, &QPushButton::clicked, this, &SerialProtocolSettingsWidget::validate);

  reloadPorts();

  for(auto rate : serial::standard_baud_rates())
    m_rate->addItem(QString::number(rate));

  QGridLayout* gLayout = new QGridLayout;

  gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  gLayout->addWidget(m_name, 0, 1, 1, 1);

  gLayout->addWidget(portLabel, 1, 0, 1, 1);
  gLayout->addWidget(m_port, 1, 1, 1, 1);

  gLayout->addWidget(rateLabel, 2, 0, 1, 1);
  gLayout->addWidget(m_rate, 2, 1, 1, 1);

  int row = 3;
#if defined(__EMSCRIPTEN__)
  m_port->setEditable(false);
  m_permissionHint = new TextLabel{{}, this};
  m_permissionHint->setWordWrap(true);

  auto chooseBtn = new QPushButton{tr("Choose a port…"), this};
  chooseBtn->setEnabled(WebSerial::available());
  connect(chooseBtn, &QPushButton::clicked, this, [this] {
    const auto act = WebSerial::userActivation();
    qDebug() << "[WebSerial] requestPort from Qt handler, navigator.userActivation ="
             << int(act);
    WebSerial::requestPort();
    pollBrowserPorts();
  });

  gLayout->addWidget(chooseBtn, row, 0, 1, 2);
  gLayout->addWidget(m_permissionHint, row + 1, 0, 1, 2);
  row += 2;

  m_pollTimer = new QTimer{this};
  m_pollTimer->setInterval(250);
  connect(m_pollTimer, &QTimer::timeout, this, [this] { pollBrowserPorts(); });
  m_pollTimer->start();
  WebSerial::scan();
  pollBrowserPorts();
#endif

  gLayout->addWidget(m_splitter, row, 0, 1, 2);
  gLayout->addWidget(validateBtn, row + 1, 0, 1, 2);

  setLayout(gLayout);

  setDefaults();
}

void SerialProtocolSettingsWidget::reloadPorts()
{
  const auto previous = m_port->currentText();
  m_port->clear();
  for(const auto& port : serial::available_ports())
    m_port->addItem(QString::fromStdString(port.port_name));

  if(!previous.isEmpty())
  {
    const int idx = m_port->findText(previous);
    if(idx >= 0)
      m_port->setCurrentIndex(idx);
  }
}

#if defined(__EMSCRIPTEN__)
void SerialProtocolSettingsWidget::pollBrowserPorts()
{
  if(!WebSerial::available())
  {
    m_permissionHint->setText(
        tr("This browser has no Web Serial support: serial devices are only "
           "available in Chromium-based browsers."));
    m_pollTimer->stop();
    return;
  }

  const int gen = WebSerial::generation();
  if(gen != m_generation)
  {
    m_generation = gen;
    WebSerial::refresh();
    reloadPorts();
  }

  if(WebSerial::requestPending())
    m_permissionHint->setText(
        tr("Waiting for the browser port chooser: click anywhere in the page."));
  else if(m_port->count() == 0)
    m_permissionHint->setText(
        tr("The browser only exposes ports you have granted access to. Use "
           "\"Choose a port…\" to pick one."));
  else
    m_permissionHint->setText({});
}
#endif

void SerialProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_codeEdit);

  m_name->setText("newDevice");
  m_codeEdit->setPlainText("");
  m_port->setCurrentIndex(0);
  m_rate->setCurrentText("9600");
}

void SerialProtocolSettingsWidget::validate()
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
        qDebug() << "Serial:" << errors;
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

Device::DeviceSettings SerialProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_name->text();
  s.protocol = SerialProtocolFactory::static_concreteKey();

  SerialSpecificSettings specific;
  const auto& ports = serial::available_ports();
  const auto& current_ui_port = m_port->currentText().toStdString();
  for(const auto& port : ports)
    if(port.port_name == current_ui_port)
      specific.port = port;
  if(specific.port.port_name.empty())
  {
    QFileInfo ff{m_port->currentText()};
    auto port_fd = ff.canonicalFilePath().toStdString();

    for(const auto& port : ports)
    {
      if(port.system_location == port_fd)
        specific.port = port;
    }
  }

  specific.rate = m_rate->currentText().toInt();
  if(specific.rate <= 0)
  {
    specific.rate = 9600;
  }
  specific.text = m_codeEdit->toPlainText();

  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void SerialProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
  SerialSpecificSettings specific;
  if(settings.deviceSpecificSettings.canConvert<SerialSpecificSettings>())
  {
    specific = settings.deviceSpecificSettings.value<SerialSpecificSettings>();

    int32_t rate{specific.rate};
    if(rate <= 0)
      rate = 9600;

    m_port->setCurrentText(QString::fromStdString(specific.port.port_name));
    m_rate->setCurrentText(QString::number(rate));
    m_codeEdit->setPlainText(specific.text);
  }
}
}
#endif
