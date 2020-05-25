// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <score/actions/Menu.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/settings/SettingsView.hpp>
#include <core/view/FixedTabWidget.hpp>
#include <core/view/QRecentFilesMenu.h>
#include <core/view/Window.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QMenuBar>
#include <QObject>
#include <QPainter>
#include <QToolBar>
#include <qnamespace.h>

#include <wobjectimpl.h>

#include <vector>

#include <unordered_map>
W_OBJECT_IMPL(score::Presenter)
namespace score
{

static auto get_menubar(View* view)
{
#ifdef __APPLE__
  return view ? new QMenuBar : (QMenuBar*)nullptr;
#else
  return view ? view->menuBar() : (QMenuBar*)nullptr;
#endif
}
Presenter::Presenter(
    const score::ApplicationSettings& app,
    score::Settings& set,
    score::ProjectSettings& pset,
    View* view,
    QObject* arg_parent)
    : QObject{arg_parent}
    , m_view{view}
    , m_settings{set}
    , m_projectSettings{pset}
    , m_docManager{view, this}
    , m_components{}
    , m_components_readonly{m_components}
    , m_menubar{get_menubar(view)}
    , m_context{
          app,
          m_components_readonly,
          m_docManager,
          m_menus,
          m_toolbars,
          m_actions,
          m_settings.settings(),
          m_view}
{
  m_docManager.init(m_context); // It is necessary to break
  // this dependency cycle.

  connect(
      &m_context.docManager, &DocumentManager::documentChanged, &m_actions, &ActionManager::reset);

  if (m_view)
    m_view->setPresenter(this);
}

bool Presenter::exit()
{
  return m_docManager.closeAllDocuments(m_context);
}

View* Presenter::view() const
{
  return m_view;
}

void Presenter::setupGUI()
{
  // TODO remove current menus / toolbars.

  // 1. Show the menus
  // If the menu has no parent menu, we add it to the main menu bar.
  {
    std::vector<Menu> menus;
    menus.reserve(m_menus.get().size());
    for (auto& elt : m_menus.get())
    {
      if (elt.second.toplevel())
        menus.push_back(elt.second);
    }
    std::sort(menus.begin(), menus.end(), [](auto& lhs, auto& rhs) {
      return lhs.column() < rhs.column();
    });

    if (view())
    {
      for (Menu& menu : menus)
      {
        view()->menuBar()->addMenu(menu.menu());
      }
    }
  }

  // 2. Show the toolbars
  // Put them in a matrix corresponding to their organization
  {
    std::unordered_map<Qt::ToolBarArea, std::vector<Toolbar>> toolbars;

    for (auto& tb : m_toolbars.get())
    {
      toolbars[(Qt::ToolBarArea)tb.second.row()].push_back(tb.second);
    }

    if (!view())
      return;

    for (auto& tb : toolbars)
    {
      ossia::sort(tb.second, [](auto& lhs, auto& rhs) { return lhs.column() < rhs.column(); });
    }

    {
      for (const Toolbar& tb : toolbars[Qt::TopToolBarArea])
      {
        view()->addTopToolbar(tb.toolbar());
      }
    }

    {
      auto bw = view()->transportBar;
      auto bl = (QGridLayout*)bw->layout();

      int i = 0;
      for (const Toolbar& tb : toolbars[Qt::BottomToolBarArea])
      {
        if (i == 2 || i == ((int)toolbars[Qt::BottomToolBarArea].size()) * 2)
        { // for 3nd and penultimate
          auto dummy = new QWidget;
          dummy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
          bl->addWidget(dummy, 0, i, 1, 1);
          i++;
          i++;
        }

        bl->addWidget(tb.toolbar(), 0, i, Qt::AlignCenter);
        tb.toolbar()->setIconSize({24, 24});
        tb.toolbar()->setFloatable(false);
        tb.toolbar()->setMovable(false);

        QPalette pal;
        pal.setColor(QPalette::Window, Qt::transparent);
        tb.toolbar()->setPalette(pal);

        i++;

        auto sp = new QWidget;
        sp->setFixedSize(10, 10);
        bl->addWidget(sp, 0, i, Qt::AlignCenter);
        i++;
      }

      bl->addWidget(view()->bottomTabs->toolbar(), 0, i, Qt::AlignRight);
    }
  }
}

void Presenter::optimize()
{
  score::optimize_hash_map(m_components.commands);
  auto& com = m_components.commands;
  auto com_end = com.end();
  for (auto it = com.begin(); it != com_end; ++it)
  {
    score::optimize_hash_map(it.value());
  }

  score::optimize_hash_map(m_components.factories);
  for (auto& fact : m_components.factories)
  {
    fact.second->optimize();
  }

  score::optimize_hash_map(m_menus.get());
  score::optimize_hash_map(m_actions.get());
  score::optimize_hash_map(m_toolbars.get());
}
}
