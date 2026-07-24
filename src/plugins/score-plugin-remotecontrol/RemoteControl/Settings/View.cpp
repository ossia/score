#include "View.hpp"
#include "score/widgets/MarginLess.hpp"

#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>

#include <wobjectimpl.h>
W_OBJECT_IMPL(RemoteControl::Settings::View)
namespace RemoteControl
{
namespace Settings
{

View::View()
{
  m_widg = new score::FormWidget{tr("Remote control")};
  auto lay = m_widg->layout();

  {
    m_enabled = new QCheckBox{tr("Enabled")};

    connect(m_enabled
            , SignalUtils::QCheckBox_checkStateChanged()
            , this
            , [&](int t) {
      switch(t)
      {
        case Qt::Unchecked:
          enabledChanged(false);
          m_web_ui->setEnabled(false);
          break;
        case Qt::Checked:
          enabledChanged(true);
          m_web_ui->setEnabled(true);
          break;
        default:
          break;
      }
    });

    lay->addRow(m_enabled);
  }

  {
    m_web_ui = new score::FormWidget{tr("Web UI")};
    auto web_lay = m_web_ui->layout();

    auto subw{new QWidget};
    auto sublay{new score::MarginLess<QHBoxLayout>{subw}};
    m_web_ui_path = new QLineEdit{};

    auto browse{new QPushButton{tr("Browse...")}};
    browse->setMaximumWidth(100);

    connect(browse, &QPushButton::clicked, this, [this]
    {
      auto f{QFileDialog::getExistingDirectory(
          nullptr, tr("Web UI folder"), m_web_ui_path->displayText())};
      if (!f.isEmpty()) webUiPathChanged(f);
    });

    sublay->addWidget(m_web_ui_path);
    sublay->addWidget(browse);
    web_lay->addRow(tr("Web UI folder"), subw);

    subw = new QWidget;
    sublay = new score::MarginLess<QHBoxLayout>{subw};
    m_server_address = new QLineEdit{};
    m_server_address->setPlaceholderText("0.0.0.0");

    connect(m_server_address
            , &QLineEdit::textEdited
            , this, [&](const QString& str)
    { if (!str.isEmpty()) serverAddressChanged(str); });

    m_server_port = new QSpinBox{};
    m_server_port->setRange(0, 99999);
    m_server_port->setMaximumWidth(100);

    connect(m_server_port
            , SignalUtils::QSpinBox_valueChanged_int()
            , this
            , [&](int t)
    { serverPortChanged(t); });

    sublay->addWidget(m_server_address);
    sublay->addWidget(m_server_port);
    web_lay->addRow(tr("HTTP server address/port"), subw);

    m_server_enabled = new QCheckBox{tr("Enable HTTP server")};

    connect(m_server_enabled
            , SignalUtils::QCheckBox_checkStateChanged()
            , this
            , [&](int t) {
      switch(t)
      {
        case Qt::Unchecked:
          serverEnabledChanged(false);
          break;
        case Qt::Checked:
          serverEnabledChanged(true);
          break;
        default:
          break;
      }
    });

    web_lay->addRow(m_server_enabled);
    lay->addRow(m_web_ui);
  }
}

void View::setEnabled(bool val)
{
  switch(m_enabled->checkState())
  {
    case Qt::Unchecked:
      if(val)
        m_enabled->setChecked(true);
      break;
    case Qt::Checked:
      if(!val)
        m_enabled->setChecked(false);
      break;
    default:
      break;
  }
}

void View::setWebUiPath(const QString& val)
{
  if (val != m_web_ui_path->displayText())
    m_web_ui_path->setText(val);
}

void View::setServerAddress(const QString& val)
{
  if (val != m_server_address->displayText())
    m_server_address->setText(val);
}

void View::setServerPort(unsigned short val)
{
  if (val != m_server_port->value())
    m_server_port->setValue(val);
}

void View::setServerEnabled(bool val)
{
  switch(m_server_enabled->checkState())
  {
    case Qt::Unchecked:
      if(val)
        m_server_enabled->setChecked(true);
      break;
    case Qt::Checked:
      if(!val)
        m_server_enabled->setChecked(false);
      break;
    default:
      break;
  }
}

QWidget* View::getWidget()
{
  return m_widg;
}

}
}
