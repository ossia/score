// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExplorerView.hpp"
#include "ExplorerModel.hpp"
#include <QCheckBox>
#include <QFormLayout>
#include <score/widgets/SignalUtils.hpp>
namespace Explorer::Settings
{
View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  SETTINGS_UI_COMBOBOX_SETUP("Log level", LogLevel, DeviceLogLevel{});


  m_widg->setLayout(lay);

  m_cb = new QCheckBox;
  lay->addRow(tr("Enable local tree"), m_cb);

  connect(m_cb, &QCheckBox::stateChanged, this, &View::localTreeChanged);
}

void View::setLocalTree(bool val)
{
  if (val != m_cb->checkState())
    m_cb->setCheckState(val ? Qt::Checked : Qt::Unchecked);
}

QWidget* View::getWidget()
{
  return m_widg;
}

SETTINGS_UI_COMBOBOX_IMPL(LogLevel)
}
