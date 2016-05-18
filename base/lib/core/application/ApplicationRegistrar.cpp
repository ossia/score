#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/view/View.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>
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
        ApplicationComponentsData& comp,
        const iscore::ApplicationContext& ctx,
        iscore::View& view,
        MenubarManager& menubar,
        std::vector<OrderedToolbar>& toolbars):
    m_components{comp},
    m_context{ctx},
    m_view{view},
    m_menubar{menubar},
    m_toolbars{toolbars}
{

}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerPlugins(
        const QStringList& pluginFiles,
        const std::vector<iscore::Plugin_QtInterface*>& vec)
{
    // We need a list for all the plug-ins present on the system even if we do not load them.
    // Else we can't blacklist / unblacklist plug-ins.
    m_components.pluginFiles = pluginFiles;
    m_components.plugins = vec;
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerApplicationContextPlugin(
        GUIApplicationContextPlugin* ctrl)
{
    // GUI Presenter stuff...
    ctrl->populateMenus(&m_menubar);
    auto toolbars = ctrl->makeToolbars();
    m_toolbars.insert(m_toolbars.end(), toolbars.begin(), toolbars.end());

    con(m_view, &iscore::View::activeWindowChanged,
           [=] () {
        ctrl->on_activeWindowChanged();
        // TODO give a context if it is deleted

    });

    m_components.appPlugins.push_back(ctrl);
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerPanel(
        PanelDelegateFactory& factory)
{
    auto panel = factory.make(m_context);
    m_view.setupPanel(panel.get());

    m_components.panels.push_back(std::move(panel));
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerCommands(
        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds)
{
    m_components.commands = std::move(cmds);
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerCommands(
        std::pair<CommandParentFactoryKey, CommandGeneratorMap>&& cmds)
{
    m_components.commands.insert(std::move(cmds));
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerFactories(
        std::unordered_map<iscore::AbstractFactoryKey, std::unique_ptr<FactoryListInterface>>&& facts)
{
    m_components.factories = std::move(facts);
}

ISCORE_LIB_BASE_EXPORT
void ApplicationRegistrar::registerFactory(std::unique_ptr<FactoryListInterface> cmds)
{
    m_components.factories.insert(std::make_pair(cmds->abstractFactoryKey(), std::move(cmds)));
}

}
