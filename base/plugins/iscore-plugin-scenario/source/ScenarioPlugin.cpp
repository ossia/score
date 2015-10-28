#include <ScenarioPlugin.hpp>
#include "Document/Event/ExecutionStatus.hpp"
#include <Control/ScenarioControl.hpp>
#include <Process/ScenarioFactory.hpp>
#include <Panel/ProcessPanelFactory.hpp>

#include <State/Message.hpp>
#include "Control/Menus/ScenarioCommonContextMenuFactory.hpp"

#include <Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>

#if defined(ISCORE_LIB_INSPECTOR)
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
    QMetaType::registerComparators<iscore::Value>();
    QMetaType::registerComparators<iscore::Message>();
    QMetaType::registerComparators<iscore::MessageList>();
    /*
    qRegisterMetaTypeStreamOperators<iscore::State>();
    qRegisterMetaTypeStreamOperators<iscore::StateList>();
    */qRegisterMetaTypeStreamOperators<iscore::Message>();
    qRegisterMetaTypeStreamOperators<iscore::MessageList>();
    qRegisterMetaTypeStreamOperators<iscore::Address>();
    qRegisterMetaTypeStreamOperators<iscore::Value>();
    qRegisterMetaTypeStreamOperators<iscore::ValueList>();

    qRegisterMetaTypeStreamOperators<TimeValue>();
    qRegisterMetaType<ExecutionStatus>();
}

// Interfaces implementations :
#include "Document/BaseElement/ScenarioDocument.hpp"
QList<iscore::DocumentDelegateFactoryInterface*> iscore_plugin_scenario::documents()
{
    return {new ScenarioDocument};
}

iscore::PluginControlInterface* iscore_plugin_scenario::make_control(iscore::Presenter* pres)
{
    return ScenarioControl::instance(pres);
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
            {ProcessFactory::factoryName(),
             [&] (iscore::FactoryInterface* fact)
             { ScenarioControl::instance()->processList()->registerProcess(fact); }
            },
            {MoveEventFactoryInterface::factoryName(),
             [&] (iscore::FactoryInterface* fact)
             { ScenarioControl::instance()->moveEventList()->registerMoveEventFactory(fact); }
            },
            {ScenarioActionsFactory::factoryName(),
             [&] (iscore::FactoryInterface* fact)
             {
                auto context_menu_fact = static_cast<ScenarioActionsFactory*>(fact);
                for(auto& act : context_menu_fact->make(ScenarioControl::instance()))
                {
                    ScenarioControl::instance()->pluginActions().push_back(act);
                }
             }
            }
           };
}

QVector<iscore::FactoryInterface*> iscore_plugin_scenario::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new ScenarioFactory};
    }

    if(factoryName == ScenarioActionsFactory::factoryName())
    {
        // new ScenarioCommonActionsFactory is instantiated in Control
        // because other plug ins need it.
        return {};
    }

    if(factoryName == MoveEventClassicFactory::factoryName())
    {
        return {new MoveEventClassicFactory};
    }

#if defined(ISCORE_LIB_INSPECTOR)
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


QStringList iscore_plugin_scenario::required() const
{
    return {};
}

QStringList iscore_plugin_scenario::offered() const
{
    return {"Scenario"};
}
