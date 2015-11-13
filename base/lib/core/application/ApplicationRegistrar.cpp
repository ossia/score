#include "ApplicationRegistrar.hpp"
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <iscore/plugins/panel/PanelFactory.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>

namespace iscore
{

ApplicationRegistrar::ApplicationRegistrar(Presenter& p):
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


    m_controls.push_back(ctrl);
}

void ApplicationRegistrar::registerPanel(
        PanelFactory* factory)
{
    auto view = factory->makeView(m_presenter.view());
    auto pres = factory->makePresenter(&m_presenter, view);

    m_panelPresenters.push_back({pres, factory});

    m_presenter.view()->setupPanelView(view);

    for(auto doc : m_presenter.documentManager().documents())
        doc->setupNewPanel(factory);
}

void ApplicationRegistrar::registerDocumentDelegate(
        DocumentDelegateFactoryInterface* docpanel)
{
    m_availableDocuments.push_back(docpanel);
}

void ApplicationRegistrar::registerCommands(
        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds)
{
    m_commands = std::move(cmds);
}

iscore::SerializableCommand* ApplicationRegistrar::instantiateUndoCommand(
        const CommandParentFactoryKey& parent_name,
        const CommandFactoryKey& name,
        const QByteArray& data)
{
    auto it = m_commands.find(parent_name);
    if(it != m_commands.end())
    {
        auto it2 = it->second.find(name);
        if(it2 != it->second.end())
        {
            return (*it2->second)(data);
        }
    }

#if defined(ISCORE_DEBUG)
    qDebug() << "ALERT: Command"
             << parent_name
             << "::"
             << name
             << "could not be instantiated.";
    ISCORE_ABORT;
#else
    throw MissingCommandException(parent_name, name);
#endif
    return nullptr;
}


}
