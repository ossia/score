// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Protocols/Settings/Model.hpp>
#include <Protocols/Settings/View.hpp>

#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
namespace Protocols::Settings
{
View::View()
{
  m_widg = new score::FormWidget{tr("Protocols")};
  auto lay = m_widg->layout();

  // General settings
  SETTINGS_UI_COMBOBOX_SETUP("Midi API", MidiAPI, MidiAPI{});
}

QWidget* View::getWidget()
{
  return m_widg;
}

SETTINGS_UI_COMBOBOX_IMPL(MidiAPI)
}
