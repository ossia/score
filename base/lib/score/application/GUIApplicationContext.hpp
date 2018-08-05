#pragma once
#include <score/application/ApplicationContext.hpp>

class QMainWindow;
namespace score
{
/**
 * @brief Specializes ApplicationContext with the QMainWindow
 *
 * We want to keep it separate in case we do a completely different UI
 * not based on Qt widgets in a few years, but instead on QQuickWindow or even
 * a web ui if it has gotten fast enough.
 */
struct GUIApplicationContext : public score::ApplicationContext
{
  explicit GUIApplicationContext(
      const score::ApplicationSettings& a,
      const ApplicationComponents& b,
      DocumentManager& c,
      score::MenuManager& d,
      score::ToolbarManager& e,
      score::ActionManager& f,
      const std::vector<std::unique_ptr<score::SettingsDelegateModel>>& g,
      QMainWindow* mw);

  /**
   * @brief List of the available GUI panels.
   *
   * @see score::PanelDelegate
   */
  auto panels() const
  {
    return components.panels();
  }

  /**
   * @brief Access to a specific PanelDelegate
   *
   * @see score::PanelDelegate
   */
  template <typename T>
  T& panel() const
  {
    return components.panel<T>();
  }
  template <typename T>
  T* findPanel() const
  {
    return components.findPanel<T>();
  }

  /**
   * @brief List of all the application-wide plug-ins
   *
   * @see score::GUIApplicationPlugin
   */
  const auto& applicationPlugins() const
  {
    return components.applicationPlugins();
  }

  /**
   * @brief List of all the gui application-wide plug-ins
   *
   * @see score::GUIApplicationPlugin
   */
  const auto& guiApplicationPlugins() const
  {
    return components.guiApplicationPlugins();
  }

  /**
   * @brief Access a specific application plug-in instance.
   *
   * @see score::GUIApplicationPlugin
   */
  template <typename T>
  T& applicationPlugin() const
  {
    return components.applicationPlugin<T>();
  }

  template <typename T>
  T* findApplicationPlugin() const
  {
    return components.findApplicationPlugin<T>();
  }

  /**
   * @brief Access a specific gui application plug-in instance.
   *
   * @see score::GUIApplicationPlugin
   */
  template <typename T>
  T& guiApplicationPlugin() const
  {
    return components.guiApplicationPlugin<T>();
  }

  template <typename T>
  T* findGuiApplicationPlugin() const
  {
    return components.findGuiApplicationPlugin<T>();
  }

  DocumentManager& docManager;

  MenuManager& menus;
  ToolbarManager& toolbars;
  ActionManager& actions;
  QMainWindow* mainWindow{};
};

SCORE_LIB_BASE_EXPORT const GUIApplicationContext& GUIAppContext();
}
