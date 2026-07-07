#include "View.hpp"

#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QFormLayout>

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

    connect(m_enabled, SignalUtils::QCheckBox_checkStateChanged(), this, [&](int t) {
      switch(t)
      {
        case Qt::Unchecked:
          enabledChanged(false);
          break;
        case Qt::Checked:
          enabledChanged(true);
          break;
        default:
          break;
      }
    });

    lay->addRow(m_enabled);
  }

  {
    m_server_enabled = new QCheckBox{tr("Enable HTTP server")};

    connect(m_server_enabled, SignalUtils::QCheckBox_checkStateChanged(), this, [&](int t) {
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

    lay->addRow(m_server_enabled);
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
