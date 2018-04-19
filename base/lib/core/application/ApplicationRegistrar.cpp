// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationRegistrar.hpp"

#include "ApplicationSettings.hpp"

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/view/Window.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <type_traits>
#include <vector>

namespace score
{
SCORE_LIB_BASE_EXPORT
ApplicationRegistrar::ApplicationRegistrar(ApplicationComponentsData& comp)
    : m_components{comp}
{
}

ApplicationRegistrar::~ApplicationRegistrar()
{
}

SCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerAddons(std::vector<score::Addon> vec)
{
  m_components.addons = std::move(vec);
}

void ApplicationRegistrar::registerApplicationPlugin(ApplicationPlugin* ctrl)
{
  m_components.appPlugins.push_back(ctrl);
}

SCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerCommands(
    score::hash_map<CommandGroupKey, CommandGeneratorMap>&& cmds)
{
  m_components.commands = std::move(cmds);
}

SCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerCommands(
    std::pair<CommandGroupKey, CommandGeneratorMap>&& cmds)
{
  m_components.commands.insert(std::move(cmds));
}

SCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerFactories(
    score::hash_map<score::InterfaceKey, std::unique_ptr<InterfaceListBase>>&&
        facts)
{
  m_components.factories = std::move(facts);
}

SCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerFactory(
    std::unique_ptr<InterfaceListBase> cmds)
{
  m_components.factories.insert(
      std::make_pair(cmds->interfaceKey(), std::move(cmds)));
}

GUIApplicationRegistrar::GUIApplicationRegistrar(
    ApplicationComponentsData& comp,
    const score::GUIApplicationContext& ctx,
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

GUIApplicationRegistrar::~GUIApplicationRegistrar()
{
}

SCORE_LIB_BASE_EXPORT
void GUIApplicationRegistrar::registerGUIApplicationPlugin(
    GUIApplicationPlugin* ctrl)
{
  if (m_context.applicationSettings.gui)
  {
    // GUI Presenter stuff...
    auto ui = ctrl->makeGUIElements();
    m_menuManager.insert(std::move(ui.menus));
    m_toolbarManager.insert(std::move(ui.toolbars));
    m_actionManager.insert(std::move(ui.actions.container));
  }
  m_components.guiAppPlugins.push_back(ctrl);
}

SCORE_LIB_BASE_EXPORT
void GUIApplicationRegistrar::registerPanel(PanelDelegateFactory& factory)
{
  auto panel = factory.make(m_context);

  m_components.panels.push_back(std::move(panel));
}
}
