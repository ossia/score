#pragma once
#include <QObject>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/Addon.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <utility>

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

#include <iscore/actions/Action.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
class DocumentDelegateFactory;
class FactoryListInterface;
class GUIApplicationContextPlugin;
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
class ISCORE_LIB_BASE_EXPORT ApplicationRegistrar : public QObject
{
public:
  ApplicationRegistrar(
      ApplicationComponentsData&,
      const iscore::ApplicationContext&,
      iscore::View&,
      MenuManager&,
      ToolbarManager&,
      ActionManager&);

  // Register data from plugins
  void registerAddons(std::vector<iscore::Addon> vec);
  void registerApplicationContextPlugin(GUIApplicationContextPlugin*);
  void registerPanel(PanelDelegateFactory&);
  void registerCommands(
      iscore::hash_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
  void registerCommands(
      std::pair<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
  void registerFactories(
      iscore::hash_map<iscore::AbstractFactoryKey, std::unique_ptr<FactoryListInterface>>&&
              cmds);
  void registerFactory(std::unique_ptr<FactoryListInterface> cmds);

  ApplicationComponentsData& components() const
  {
    return m_components;
  }

private:
  ApplicationComponentsData& m_components;
  const iscore::ApplicationContext& m_context;
  iscore::View& m_view;

  MenuManager& m_menuManager;
  ToolbarManager& m_toolbarManager;
  ActionManager& m_actionManager;
};
}
