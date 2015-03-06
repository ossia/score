#include <ScenarioPlugin.hpp>
#include <QStringList>
#include <Control/ScenarioControl.hpp>
#include <Process/ScenarioFactory.hpp>

#include <Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Inspector/Event/EventInspectorFactory.hpp>
#include <Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Inspector/TimeNode/TimeNodeInspectorFactory.hpp>

ScenarioPlugin::ScenarioPlugin() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {},
        iscore::DocumentDelegateFactoryInterface_QtInterface {},
        iscore::FactoryFamily_QtInterface {},
        iscore::FactoryInterface_QtInterface {},
m_control {new ScenarioControl{nullptr}}
{
    setObjectName("ScenarioPlugin");
}

// Interfaces implementations :
QStringList ScenarioPlugin::document_list() const
{
    return {"Scenario document"};
}

#include "Document/BaseElement/ScenarioDocument.hpp"
iscore::DocumentDelegateFactoryInterface* ScenarioPlugin::document_make(QString name)
{
    if(name == QString("Scenario document"))
    {
        return new ScenarioDocument;
    }

    return nullptr;
}

QStringList ScenarioPlugin::control_list() const
{
    return {"Scenario control"};
}

iscore::PluginControlInterface* ScenarioPlugin::control_make(QString name)
{
    if(name == "Scenario control")
    {
        return m_control;
    }

    return nullptr;
}

QVector<iscore::FactoryFamily> ScenarioPlugin::factoryFamilies_make()
{
    return {{"Process",
            std::bind(&ProcessList::addProcess,
            m_control->processList(),
            std::placeholders::_1)
        }
    };
}

QVector<iscore::FactoryInterface*> ScenarioPlugin::factories_make(QString factoryName)
{
    if(factoryName == "Process")
    {
        return {new ScenarioFactory};
    }

    if(factoryName == "Inspector")
    {
        return {new ConstraintInspectorFactory,
                   new EventInspectorFactory,
                   new ScenarioInspectorFactory,
                   new TimeNodeInspectorFactory
        };
    }

    return {};
}
