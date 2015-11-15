#include "ApplicationRegistrar.hpp"
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <iscore/plugins/panel/PanelFactory.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <iscore/tools/exceptions/MissingCommand.hpp>
#include <core/application/ApplicationComponents.hpp>
namespace iscore
{

ApplicationRegistrar::ApplicationRegistrar(ApplicationComponentsData& c, Presenter& p):
    m_components{c},
    m_presenter{p}
{

}

void ApplicationRegistrar::registerPluginControl(
        PluginControlInterface* ctrl)
{
    ctrl->setParent(&m_presenter); // TODO replace by some ApplicationContext...

    // GUI Presenter stuff...
    ctrl->populateMenus(&m_presenter.menuBar());
    m_presenter.toolbars() += ctrl->makeToolbars();

    m_components.controls.push_back(ctrl);
}

void ApplicationRegistrar::registerPanel(
        PanelFactory* factory)
{
    auto view = factory->makeView(m_presenter.view());
    auto pres = factory->makePresenter(&m_presenter, view);

    m_components.panelPresenters.push_back({pres, factory});

    m_presenter.view()->setupPanelView(view);

    for(auto doc : m_presenter.documentManager().documents())
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



}
