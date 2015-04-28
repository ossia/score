#include "PresenterInterface.hpp"
#include <QApplication>
#include <core/presenter/Presenter.hpp>
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

iscore::SerializableCommand*
iscore::IPresenter::instantiateUndoCommand(const QString& parent_name,
        const QString& name,
        const QByteArray& data)
{
    auto presenter = qApp->findChild<iscore::Presenter*> ("Presenter");
    return presenter->instantiateUndoCommand(parent_name,
            name,
            data);
}



QList<iscore::PanelFactoryInterface*>
iscore::IPresenter::panelFactories()
{
    return qApp->findChild<iscore::Presenter*> ("Presenter")->panelFactories();
}



const std::vector<iscore::PluginControlInterface*>& iscore::IPresenter::pluginControls()
{
    return qApp->findChild<iscore::Presenter*> ("Presenter")->pluginControls();
}
