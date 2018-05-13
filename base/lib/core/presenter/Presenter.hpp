#pragma once
#include <QMenuBar>
#include <wobjectdefs.h>
#include <core/presenter/DocumentManager.hpp>
#include <core/settings/Settings.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/actions/ToolbarManager.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score_lib_base_export.h>
#include <vector>
class QObject;

namespace score
{

class CoreApplicationPlugin;
class View;
class Settings;

/**
 * @brief The Presenter class
 *
 * Certainly needs refactoring.
 * For now, manages menus and plug-in objects.
 *
 * It is also able to instantiate a Command from serialized Undo/Redo data.
 * (this should go in the DocumentPresenter maybe ?)
 */
class SCORE_LIB_BASE_EXPORT Presenter final : public QObject
{
  W_OBJECT(Presenter)
  friend class score::CoreApplicationPlugin;

public:
  Presenter(
      const score::ApplicationSettings& app,
      const score::Settings& set,
      score::ProjectSettings& pset,
      score::View* view,
      QObject* parent);

  // Exit score
  bool exit();

  View* view() const;

  auto& menuManager()
  {
    return m_menus;
  }
  auto& toolbarManager()
  {
    return m_toolbars;
  }
  auto& actionManager()
  {
    return m_actions;
  }

  // Called after all the classes
  // have been loaded from plug-ins.
  void setupGUI();

  auto& documentManager()
  {
    return m_docManager;
  }
  const ApplicationComponents& applicationComponents()
  {
    return m_components_readonly;
  }
  const GUIApplicationContext& applicationContext()
  {
    return m_context;
  }

  auto& components()
  {
    return m_components;
  }

  void optimize();

private:
  void setupMenus();
  View* m_view{};
  const Settings& m_settings;
  ProjectSettings& m_projectSettings;

  DocumentManager m_docManager;
  ApplicationComponentsData m_components;
  ApplicationComponents m_components_readonly;

  QMenuBar* m_menubar{};
  GUIApplicationContext m_context;

  MenuManager m_menus;
  ToolbarManager m_toolbars;
  ActionManager m_actions;
};
}
