#pragma once
#include <score/application/ApplicationComponents.hpp>

namespace score
{
class ApplicationComponents;
struct ApplicationSettings;
class SettingsDelegateModel;
class DocumentManager;
class MenuManager;
class ToolbarManager;
class ActionManager;
struct DocumentList;
/**
 * @brief Used to access all the application-wide state and structures
 *
 * For instance : menus, toolbars, registered factories, etc.
 *
 * Accessible through \ref score::AppContext() ; however,
 * it is better to try to get a reference to it by constructor injection.
 */
struct SCORE_LIB_BASE_EXPORT ApplicationContext
{
  explicit ApplicationContext(
      const score::ApplicationSettings&,
      const ApplicationComponents&,
      DocumentList& l,
      const std::vector<std::unique_ptr<score::SettingsDelegateModel>>&);
  ApplicationContext(const ApplicationContext&) = delete;
  ApplicationContext(ApplicationContext&&) = delete;
  ApplicationContext& operator=(const ApplicationContext&) = delete;

  ~ApplicationContext();

  /**
   * @brief Access a specific Settings model instance.
   *
   * @see score::Settings::Model
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

    SCORE_ABORT;
    throw;
  }

  /**
   * @brief List of all the registered addons.
   *
   * @see score::Addon
   */
  const auto& addons() const
  {
    return components.addons();
  }

  /**
   * @brief Access to a specific interface list
   *
   * @see score::InterfaceList
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
  const score::ApplicationSettings& applicationSettings;

  const score::ApplicationComponents& components;

  DocumentList& documents;

private:
  const std::vector<std::unique_ptr<score::SettingsDelegateModel>>& m_settings;
};

// By default this is defined in score::Application
SCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext();
}
