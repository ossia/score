// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/Presenter.hpp>
#include <Media/Effect/Settings/View.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
namespace Media::Settings
{
View::View()
{
  m_widg = new QTabWidget;
  auto& ctx = score::AppContext();
  auto& tabs = ctx.interfaces<PluginSettingsFactoryList>();

  for(auto& tab : tabs)
  {
    if(auto widg = tab.make(ctx))
    {
      m_widg->addTab(widg, tab.name());
    }
  }
}

QWidget* View::getWidget()
{
  return m_widg;
}

PluginSettingsTab::~PluginSettingsTab() { }

}
