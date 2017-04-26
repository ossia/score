#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/view/View.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>
#include <type_traits>
#include <vector>

#include "ApplicationRegistrar.hpp"
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

namespace iscore
{
ISCORE_LIB_BASE_EXPORT
ApplicationRegistrar::ApplicationRegistrar(
    ApplicationComponentsData& comp)
  : m_components{comp}
{
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerAddons(std::vector<iscore::Addon> vec)
{
  m_components.addons = std::move(vec);
}

void ApplicationRegistrar::registerApplicationPlugin(ApplicationPlugin* ctrl)
{
  m_components.appPlugins.push_back(ctrl);
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerCommands(
    iscore::hash_map<CommandGroupKey, CommandGeneratorMap>&& cmds)
{
  m_components.commands = std::move(cmds);
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerCommands(
    std::pair<CommandGroupKey, CommandGeneratorMap>&& cmds)
{
  m_components.commands.insert(std::move(cmds));
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerFactories(
    iscore::hash_map<iscore::InterfaceKey, std::unique_ptr<InterfaceListBase>>&&
            facts)
{
  m_components.factories = std::move(facts);
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerFactory(
    std::unique_ptr<InterfaceListBase> cmds)
{
  m_components.factories.insert(
      std::make_pair(cmds->interfaceKey(), std::move(cmds)));
}


GUIApplicationRegistrar::GUIApplicationRegistrar(
    ApplicationComponentsData& comp,
    const iscore::GUIApplicationContext& ctx,
    MenuManager& m,
    ToolbarManager& t,
    ActionManager& a)
    : ApplicationRegistrar{comp}
    , m_context{ctx}
    , m_menuManager{m}
    , m_toolbarManager{t}
    , m_actionManager{a}
{
}

ISCORE_LIB_BASE_EXPORT
void GUIApplicationRegistrar::registerGUIApplicationPlugin(
    GUIApplicationPlugin* ctrl)
{
  // GUI Presenter stuff...
  auto ui = ctrl->makeGUIElements();
  m_menuManager.insert(std::move(ui.menus));
  m_toolbarManager.insert(std::move(ui.toolbars));
  m_actionManager.insert(std::move(ui.actions.container));

  m_components.guiAppPlugins.push_back(ctrl);
}

ISCORE_LIB_BASE_EXPORT
void GUIApplicationRegistrar::registerPanel(PanelDelegateFactory& factory)
{
  auto panel = factory.make(m_context);

  m_components.panels.push_back(std::move(panel));
}

}
