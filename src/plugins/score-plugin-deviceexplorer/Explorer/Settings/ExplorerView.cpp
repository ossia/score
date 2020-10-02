// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExplorerView.hpp"

#include "ExplorerModel.hpp"

#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/FormWidget.hpp>

#include <QCheckBox>
#include <QFormLayout>
namespace Explorer::Settings
{
View::View()
{
  m_widg = new score::FormWidget{tr("Device explorer")};
  auto lay = m_widg->layout();

  SETTINGS_UI_COMBOBOX_SETUP("Log level", LogLevel, DeviceLogLevel{});

  m_cb = new QCheckBox{tr("Enable local tree")};
  lay->addRow(m_cb);

  connect(m_cb, &QCheckBox::stateChanged, this, [this] (int b) { localTreeChanged(b); });
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

namespace Explorer::ProjectSettings
{
View::View()
{
  m_widg = new score::FormWidget{tr("Device explorer")};
  auto lay = m_widg->layout();
  SETTINGS_UI_DOUBLE_SPINBOX_SETUP("Midi Import Ratio", MidiImportRatio);
  SETTINGS_UI_TOGGLE_SETUP("Refresh on load", RefreshOnStart);
  SETTINGS_UI_TOGGLE_SETUP("Reconnect on load", ReconnectOnStart);
}

QWidget* View::getWidget()
{
  return m_widg;
}

SETTINGS_UI_DOUBLE_SPINBOX_IMPL(MidiImportRatio)
SETTINGS_UI_TOGGLE_IMPL(RefreshOnStart)
SETTINGS_UI_TOGGLE_IMPL(ReconnectOnStart)
}
