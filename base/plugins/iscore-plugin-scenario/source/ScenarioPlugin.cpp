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

#include <ProcessInterface/TimeValue.hpp>
iscore_plugin_scenario::iscore_plugin_scenario() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {},
        iscore::DocumentDelegateFactoryInterface_QtInterface {},
        iscore::FactoryFamily_QtInterface {},
        iscore::FactoryInterface_QtInterface {}
{
    QMetaType::registerComparators<Message>();
    QMetaType::registerComparators<MessageList>();
    qRegisterMetaTypeStreamOperators<State>();
    qRegisterMetaTypeStreamOperators<StateList>();
    qRegisterMetaTypeStreamOperators<Message>();
    qRegisterMetaTypeStreamOperators<MessageList>();

    qRegisterMetaTypeStreamOperators<TimeValue>();
}

// Interfaces implementations :
#include "Document/BaseElement/ScenarioDocument.hpp"
QList<iscore::DocumentDelegateFactoryInterface*> iscore_plugin_scenario::documents()
{
    return {new ScenarioDocument};
}

iscore::PluginControlInterface* iscore_plugin_scenario::make_control(iscore::Presenter* pres)
{
    delete m_control;
    m_control = new ScenarioControl{pres};
    return m_control;
}

QList<iscore::PanelFactory*> iscore_plugin_scenario::panels()
{
    return {
        new ProcessPanelFactory
    };
}

QVector<iscore::FactoryFamily> iscore_plugin_scenario::factoryFamilies()
{
    return {{"Process",
            [&] (iscore::FactoryInterface* fact)
            { m_control->processList()->registerProcess(fact); }}};
}

QVector<iscore::FactoryInterface*> iscore_plugin_scenario::factories(const QString& factoryName)
{
    // TODO use macros for these names
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
