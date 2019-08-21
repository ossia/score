#include "View.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QSpinBox>

#include <wobjectimpl.h>
W_OBJECT_IMPL(RemoteControl::Settings::View)
namespace RemoteControl
{
namespace Settings
{

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;

  {
    m_enabled = new QCheckBox;

    connect(m_enabled, &QCheckBox::stateChanged, this, [&](int t) {
      switch (t)
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

    lay->addRow(tr("Enabled"), m_enabled);
  }

  m_widg->setLayout(lay);
}

void View::setEnabled(bool val)
{
  switch (m_enabled->checkState())
  {
    case Qt::Unchecked:
      if (val)
        m_enabled->setChecked(true);
      break;
    case Qt::Checked:
      if (!val)
        m_enabled->setChecked(false);
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
