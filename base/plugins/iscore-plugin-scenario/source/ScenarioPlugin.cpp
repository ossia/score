#include <ScenarioPlugin.hpp>
#include "Document/Event/EventStatus.hpp"
#include <Control/ScenarioControl.hpp>
#include <Process/ScenarioFactory.hpp>
#include <Panel/ProcessPanelFactory.hpp>

#include <State/State.hpp>
#include <State/Message.hpp>

#if defined(ISCORE_INSPECTOR_LIB)
#include <Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Inspector/Event/EventInspectorFactory.hpp>
#include <Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Inspector/TimeNode/TimeNodeInspectorFactory.hpp>
#include <Inspector/State/StateInspectorFactory.hpp>
#endif

iscore_plugin_scenario::iscore_plugin_scenario() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {},
        iscore::DocumentDelegateFactoryInterface_QtInterface {},
        iscore::FactoryFamily_QtInterface {},
        iscore::FactoryInterface_QtInterface {}
{
    QMetaType::registerComparators<iscore::Message>();
    QMetaType::registerComparators<iscore::MessageList>();
    qRegisterMetaTypeStreamOperators<iscore::State>();
    qRegisterMetaTypeStreamOperators<iscore::StateList>();
    qRegisterMetaTypeStreamOperators<iscore::Message>();
    qRegisterMetaTypeStreamOperators<iscore::MessageList>();

    qRegisterMetaTypeStreamOperators<TimeValue>();
    qRegisterMetaType<EventStatus>();
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
    return {
            {"Process",
             [&] (iscore::FactoryInterface* fact)
             { m_control->processList()->registerProcess(fact); }
            },
            {"ScenarioContextMenu",
             [&] (iscore::FactoryInterface* fact)
             { m_control->contextMenuList()->registerContextMenu(fact); }
            }
           };
}

QVector<iscore::FactoryInterface*> iscore_plugin_scenario::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new ScenarioFactory};
    }

#if defined(ISCORE_INSPECTOR_LIB)
    if(factoryName == InspectorWidgetFactory::factoryName())
    {
        return {
                    new ConstraintInspectorFactory,
                    new StateInspectorFactory,
                    new EventInspectorFactory,
                    new ScenarioInspectorFactory,
                    new TimeNodeInspectorFactory
        };
    }
#endif

    return {};
}
