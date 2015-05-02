#include <ScenarioPlugin.hpp>
#include <Control/ScenarioControl.hpp>
#include <Process/ScenarioFactory.hpp>
#include <Panel/ProcessPanelFactory.hpp>

#include <Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Inspector/Event/EventInspectorFactory.hpp>
#include <Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Inspector/TimeNode/TimeNodeInspectorFactory.hpp>

#include <State/State.hpp>
#include <State/Message.hpp>

ScenarioPlugin::ScenarioPlugin() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {},
        iscore::DocumentDelegateFactoryInterface_QtInterface {},
        iscore::FactoryFamily_QtInterface {},
        iscore::FactoryInterface_QtInterface {},
m_control {new ScenarioControl{nullptr}}
{
    setObjectName("ScenarioPlugin");

    QMetaType::registerComparators<Message>();
    QMetaType::registerComparators<MessageList>();
    qRegisterMetaTypeStreamOperators<State>();
    qRegisterMetaTypeStreamOperators<StateList>();
    qRegisterMetaTypeStreamOperators<Message>();
    qRegisterMetaTypeStreamOperators<MessageList>();
}

// Interfaces implementations :
#include "Document/BaseElement/ScenarioDocument.hpp"
QList<iscore::DocumentDelegateFactoryInterface*> ScenarioPlugin::documents()
{
    return {new ScenarioDocument};
}

iscore::PluginControlInterface* ScenarioPlugin::control()
{
    return m_control;
}

QList<iscore::PanelFactoryInterface*> ScenarioPlugin::panels()
{
    return {/*new ProcessPanelFactory*/};
}

QVector<iscore::FactoryFamily> ScenarioPlugin::factoryFamilies()
{
    return {{"Process",
            std::bind(&ProcessList::registerProcess,
                      m_control->processList(),
            std::placeholders::_1)}
    };
}

QVector<iscore::FactoryInterface*> ScenarioPlugin::factories(const QString& factoryName)
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
                new TimeNodeInspectorFactory};
    }

    return {};
}
