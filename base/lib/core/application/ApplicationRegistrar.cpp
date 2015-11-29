#include <core/application/Application.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/view/View.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/panel/PanelFactory.hpp>
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

ApplicationRegistrar::ApplicationRegistrar(
        ApplicationComponentsData& c,
        iscore::Application& p):
    m_components{c},
    m_app{p}
{

}

void ApplicationRegistrar::registerApplicationContextPlugin(
        GUIApplicationContextPlugin* ctrl)
{
    ctrl->setParent(&m_app.presenter()); // TODO replace by some ApplicationContext...

    // GUI Presenter stuff...
    ctrl->populateMenus(&m_app.presenter().menuBar());
    auto toolbars = ctrl->makeToolbars();
    auto& currentToolbars = m_app.presenter().toolbars();
    currentToolbars.insert(currentToolbars.end(), toolbars.begin(), toolbars.end());

    m_components.appPlugins.push_back(ctrl);
}

void ApplicationRegistrar::registerPanel(
        PanelFactory* factory)
{
    auto view = factory->makeView(iscore::ApplicationContext(m_app), m_app.presenter().view());
    auto pres = factory->makePresenter(&m_app.presenter(), view);

    m_components.panelPresenters.push_back({pres, factory});

    m_app.presenter().view()->setupPanelView(view);

    for(auto doc : m_app.presenter().documentManager().documents())
        doc->setupNewPanel(factory);
}

void ApplicationRegistrar::registerDocumentDelegate(
        DocumentDelegateFactoryInterface* docpanel)
{
    m_components.availableDocuments.push_back(docpanel);
}

void ApplicationRegistrar::registerCommands(
        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds)
{
    m_components.commands = std::move(cmds);
}

void ApplicationRegistrar::registerCommands(
        std::pair<CommandParentFactoryKey, CommandGeneratorMap>&& cmds)
{
    m_components.commands.insert(std::move(cmds));
}

void ApplicationRegistrar::registerFactories(
        std::unordered_map<iscore::FactoryBaseKey, std::unique_ptr<FactoryListInterface>>&& facts)
{
    m_components.factories = std::move(facts);
}

void ApplicationRegistrar::registerFactory(std::unique_ptr<FactoryListInterface> cmds)
{
    m_components.factories.insert(std::make_pair(cmds->name(), std::move(cmds)));
}

void ApplicationRegistrar::registerSettings(SettingsDelegateFactoryInterface* set)
{
    m_app.settings()->setupSettingsPlugin(set);
}



}
