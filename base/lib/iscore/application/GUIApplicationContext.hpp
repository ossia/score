#pragma once
#include <iscore/application/ApplicationContext.hpp>

class QMainWindow;
namespace iscore
{
/**
 * @brief Specializes ApplicationContext with the QMainWindow
 *
 * We want to keep it separate in case we do a completely different UI
 * not based on Qt widgets in a few years, but instead on QQuickWindow or even
 * a web ui if it has gotten fast enough.
 */
struct GUIApplicationContext : public iscore::ApplicationContext
{
  explicit GUIApplicationContext(
      const iscore::ApplicationSettings& a,
      const ApplicationComponents& b,
      DocumentManager& c,
      iscore::MenuManager& d,
      iscore::ToolbarManager& e,
      iscore::ActionManager& f,
      const std::vector<std::unique_ptr<iscore::SettingsDelegateModel>>& g,
      QMainWindow& mw);

  /**
   * @brief List of the available GUI panels.
   *
   * @see iscore::PanelDelegate
   */
  auto panels() const
  {
    return components.panels();
  }

  /**
   * @brief Access to a specific PanelDelegate
   *
   * @see iscore::PanelDelegate
   */
  template<typename T>
  T& panel() const
  {
    return components.panel<T>();
  }

  /**
   * @brief List of all the application-wide plug-ins
   *
   * @see iscore::GUIApplicationPlugin
   */
  const auto& applicationPlugins() const
  {
    return components.applicationPlugins();
  }

  /**
   * @brief List of all the gui application-wide plug-ins
   *
   * @see iscore::GUIApplicationPlugin
   */
  const auto& guiApplicationPlugins() const
  {
    return components.guiApplicationPlugins();
  }

  /**
   * @brief Access a specific application plug-in instance.
   *
   * @see iscore::GUIApplicationPlugin
   */
  template <typename T>
  T& applicationPlugin() const
  {
    return components.applicationPlugin<T>();
  }

  /**
   * @brief Access a specific gui application plug-in instance.
   *
   * @see iscore::GUIApplicationPlugin
   */
  template <typename T>
  T& guiApplicationPlugin() const
  {
    return components.guiApplicationPlugin<T>();
  }

  DocumentManager& docManager;

  MenuManager& menus;
  ToolbarManager& toolbars;
  ActionManager& actions;
  QMainWindow& mainWindow;
};

ISCORE_LIB_BASE_EXPORT const GUIApplicationContext& GUIAppContext();

}
