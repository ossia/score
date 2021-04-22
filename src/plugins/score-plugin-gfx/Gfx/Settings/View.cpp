// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>

#include <Gfx/Settings/Model.hpp>
#include <Gfx/Settings/Presenter.hpp>
#include <Gfx/Settings/View.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::Settings::View)
namespace Gfx::Settings
{
View::View()
{
  m_widg = new score::FormWidget{tr("Graphics")};
  auto& ctx = score::AppContext();

  auto lay = m_widg->layout();
  SETTINGS_UI_COMBOBOX_SETUP("Graphics API", GraphicsApi, GraphicsApis{});
}

QWidget* View::getWidget()
{
  return m_widg;
}

SETTINGS_UI_COMBOBOX_IMPL(GraphicsApi)

}
