#include "ApplicationRegistrar.hpp"
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <iscore/plugins/panel/PanelFactory.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/application/Application.hpp>
#include <core/view/View.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <core/settings/Settings.hpp>
namespace iscore
{

ApplicationRegistrar::ApplicationRegistrar(
        ApplicationComponentsData& c,
        iscore::Application& p):
    m_components{c},
    m_app{p}
{

}

void ApplicationRegistrar::registerPluginControl(
        PluginControlInterface* ctrl)
{
    ctrl->setParent(&m_app.presenter()); // TODO replace by some ApplicationContext...

    // GUI Presenter stuff...
    ctrl->populateMenus(&m_app.presenter().menuBar());
    auto toolbars = ctrl->makeToolbars();
    auto& currentToolbars = m_app.presenter().toolbars();
    currentToolbars.insert(currentToolbars.end(), toolbars.begin(), toolbars.end());

    m_components.controls.push_back(ctrl);
}

void ApplicationRegistrar::registerPanel(
        PanelFactory* factory)
{
    auto view = factory->makeView(m_app.presenter().view());
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
        std::unordered_map<iscore::FactoryBaseKey, FactoryListInterface*>&& facts)
{
    m_components.factories = std::move(facts);
}

void ApplicationRegistrar::registerFactory(FactoryListInterface* cmds)
{
    m_components.factories.insert(std::make_pair(cmds->name(), cmds));
}

void ApplicationRegistrar::registerSettings(SettingsDelegateFactoryInterface* set)
{
    m_app.settings()->setupSettingsPlugin(set);
}



}
