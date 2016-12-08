#pragma once
#include <iscore/application/ApplicationComponents.hpp>

namespace iscore
{
class ApplicationComponents;
struct ApplicationSettings;
class SettingsDelegateModel;
class DocumentManager;
class MenuManager;
class ToolbarManager;
class ActionManager;

struct ISCORE_LIB_BASE_EXPORT ApplicationContext
{
  explicit ApplicationContext(
      const iscore::ApplicationSettings&,
      const ApplicationComponents&,
      DocumentManager&,
      iscore::MenuManager&,
      iscore::ToolbarManager&,
      iscore::ActionManager&,
      const std::vector<std::unique_ptr<iscore::SettingsDelegateModel>>&);
  ApplicationContext(const ApplicationContext&) = delete;
  ApplicationContext(ApplicationContext&&) = delete;
  ApplicationContext& operator=(const ApplicationContext&) = delete;

  virtual ~ApplicationContext();

  /**
   * @brief settings Access a specific Settings model instance.
   *
   * @see iscore::Settings::Model
   */
  template <typename T>
  T& settings() const
  {
    for (auto& elt : this->m_settings)
    {
      if (auto c = dynamic_cast<T*>(elt.get()))
      {
        return *c;
      }
    }

    ISCORE_ABORT;
    throw;
  }

  /**
   * @brief applicationPlugins List of all the application-wide plug-ins
   *
   * @see iscore::GUIApplicationContextPlugin
   */
  const auto& applicationPlugins() const
  {
    return components.applicationPlugins();
  }

  /**
   * @brief settings Access a specific application plug-in instance.
   *
   * @see iscore::GUIApplicationContextPlugin
   */
  template <typename T>
  T& applicationPlugin() const
  {
    return components.applicationPlugin<T>();
  }

  /**
   * @brief addons List of all the registered addons.
   *
   * @see iscore::Addon
   */
  const auto& addons() const
  {
    return components.addons();
  }

  /**
   * @brief panels List of the available GUI panels.
   *
   * @see iscore::PanelDelegate
   */
  auto panels() const
  {
    return components.panels();
  }

  /**
   * @brief interfaces Access to a specific interface list
   *
   * @see iscore::InterfaceList
   */
  template <typename T>
  const T& interfaces() const
  {
    return components.interfaces<T>();
  }

  /**
   * @brief instantiateUndoCommand Is used to generate a Command from its
   * serialized data.
   * @param parent_name The name of the object able to generate the command.
   * Must be a CustomCommand.
   * @param name The name of the command to generate.
   * @param data The data of the command.
   *
   * Ownership of the command is transferred to the caller, and he must delete
   * it.
   */
  auto instantiateUndoCommand(const CommandData& cmd) const
  {
    return components.instantiateUndoCommand(cmd);
  }

  //! Access to start-up command-line settings
  const iscore::ApplicationSettings& applicationSettings;

  const iscore::ApplicationComponents& components;

  DocumentManager& documents;
  MenuManager& menus;
  ToolbarManager& toolbars;
  ActionManager& actions;

private:
  const std::vector<std::unique_ptr<iscore::SettingsDelegateModel>>&
      m_settings;
};

// By default this is defined in iscore::Application
ISCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext();
}
