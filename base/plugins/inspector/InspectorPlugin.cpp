#include "InspectorPlugin.hpp"
#include "Panel/InspectorPanelFactory.hpp"
using namespace iscore;

#include "InspectorInterface/InspectorWidgetFactoryInterface.hpp"
#include <InspectorControl.hpp>

InspectorPlugin::InspectorPlugin() :
    QObject {},
        iscore::Autoconnect_QtInterface {},
        iscore::PanelFactoryInterface_QtInterface {},
m_inspectorControl {new InspectorControl}
{
    setObjectName("InspectorPlugin");
}





QList<Autoconnect> InspectorPlugin::autoconnect_list() const
{
    return
    {
        /// Common
        /*{   {
                iscore::Autoconnect::ObjectRepresentationType::QObjectName,
                "DocumentPresenter", SIGNAL(elementSelected(QObject*))
            },
            {
                iscore::Autoconnect::ObjectRepresentationType::QObjectName,
                "InspectorPanelModel", SLOT(newItemInspected(QObject*))
            }
        },

        {   {
                iscore::Autoconnect::ObjectRepresentationType::Inheritance,
                "InspectorWidgetBase", SIGNAL(submitCommand(iscore::SerializableCommand*))
            },
            {
                iscore::Autoconnect::ObjectRepresentationType::QObjectName,
                "Presenter", SLOT(applyCommand(iscore::SerializableCommand*))
            }
        },*/

        {   {
                iscore::Autoconnect::ObjectRepresentationType::Inheritance,
                "InspectorWidgetBase", SIGNAL(initiateOngoingCommand(iscore::SerializableCommand*, QObject*))
            },
            {
                iscore::Autoconnect::ObjectRepresentationType::QObjectName,
                "DocumentPresenter", SLOT(initiateOngoingCommand(iscore::SerializableCommand*, QObject*))
            }
        },

        {   {
                iscore::Autoconnect::ObjectRepresentationType::Inheritance,
                "InspectorWidgetBase", SIGNAL(continueOngoingCommand(iscore::SerializableCommand*))
            },
            {
                iscore::Autoconnect::ObjectRepresentationType::QObjectName,
                "DocumentPresenter", SLOT(continueOngoingCommand(iscore::SerializableCommand*))
            }
        },
        {   {
                iscore::Autoconnect::ObjectRepresentationType::Inheritance,
                "InspectorWidgetBase", SIGNAL(undoOngoingCommand())
            },
            {
                iscore::Autoconnect::ObjectRepresentationType::QObjectName,
                "DocumentPresenter", SLOT(rollbackOngoingCommand())
            }
        },
        {   {
                iscore::Autoconnect::ObjectRepresentationType::Inheritance,
                "InspectorWidgetBase", SIGNAL(validateOngoingCommand())
            },
            {
                iscore::Autoconnect::ObjectRepresentationType::QObjectName,
                "DocumentPresenter", SLOT(validateOngoingCommand())
            }
        },

    };
}



QStringList InspectorPlugin::panel_list() const
{
    return {"Inspector Panel"};
}

PanelFactoryInterface* InspectorPlugin::panel_make(QString name)
{
    if(name == "Inspector Panel")
    {
        return new InspectorPanelFactory;
    }

    return nullptr;
}

QVector<FactoryFamily> InspectorPlugin::factoryFamilies_make()
{
    return {{"Inspector",
            std::bind(&InspectorControl::on_newInspectorWidgetFactory,
            m_inspectorControl,
            std::placeholders::_1)
        }
    };
}


QStringList InspectorPlugin::control_list() const
{
    return {"InspectorControl"};
}

PluginControlInterface* InspectorPlugin::control_make(QString s)
{
    if(s == "InspectorControl")
    {
        return m_inspectorControl;
    }

    return nullptr;
}
