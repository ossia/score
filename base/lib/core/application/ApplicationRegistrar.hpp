#pragma once
#include <QObject>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/Addon.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <utility>

#include <iscore/command/Command.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

#include <iscore/actions/Action.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
class DocumentDelegateFactory;
class InterfaceListBase;
struct GUIApplicationContext;
class GUIApplicationPlugin;
class PanelDelegateFactory;
class SettingsDelegateFactory;
struct ApplicationComponentsData;
class View;
class Settings;
class Plugin_QtInterface;

/**
 * @brief Registers the plug-in elements.
 *
 * Used at the plug-in loading time : stores the
 * classes of the plug-in and performs minor initialization steps.
 */

class ISCORE_LIB_BASE_EXPORT ApplicationRegistrar
    : public QObject
{
public:
  ApplicationRegistrar(
      ApplicationComponentsData&);

  void registerAddons(std::vector<iscore::Addon> vec);
  void registerApplicationPlugin(ApplicationPlugin*);

  void registerCommands(
      iscore::hash_map<CommandGroupKey, CommandGeneratorMap>&& cmds);
  void registerCommands(
      std::pair<CommandGroupKey, CommandGeneratorMap>&& cmds);
  void registerFactories(
      iscore::hash_map<iscore::InterfaceKey, std::unique_ptr<InterfaceListBase>>&&
              cmds);
  void registerFactory(std::unique_ptr<InterfaceListBase> cmds);

  ApplicationComponentsData& components() const
  {
    return m_components;
  }

protected:
  ApplicationComponentsData& m_components;
};

class ISCORE_LIB_BASE_EXPORT GUIApplicationRegistrar
    : public ApplicationRegistrar
{
public:
  GUIApplicationRegistrar(
      ApplicationComponentsData&,
      const iscore::GUIApplicationContext&,
      MenuManager&,
      ToolbarManager&,
      ActionManager&);

  // Register data from plugins
  void registerGUIApplicationPlugin(GUIApplicationPlugin*);
  void registerPanel(PanelDelegateFactory&);

private:
  const iscore::GUIApplicationContext& m_context;

  MenuManager& m_menuManager;
  ToolbarManager& m_toolbarManager;
  ActionManager& m_actionManager;
};
}
