#include <Scenario/ScenarioPlugin.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Control/ScenarioControl.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Panel/ProcessPanelFactory.hpp>

#include <State/Message.hpp>
#include <Scenario/Control/Menus/ScenarioCommonContextMenuFactory.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <core/application/Application.hpp>
#include <Scenario/Document/BaseElement/ScenarioDocument.hpp>

#if defined(ISCORE_LIB_INSPECTOR)
#include <Scenario/Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Scenario/Inspector/Event/EventInspectorFactory.hpp>
#include <Scenario/Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Scenario/Inspector/TimeNode/TimeNodeInspectorFactory.hpp>
#include <Scenario/Inspector/State/StateInspectorFactory.hpp>
#endif

iscore_plugin_scenario::iscore_plugin_scenario() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {},
        iscore::DocumentDelegateFactoryInterface_QtInterface {},
        iscore::FactoryList_QtInterface {},
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
QList<iscore::DocumentDelegateFactoryInterface*> iscore_plugin_scenario::documents()
{
    return {new ScenarioDocument};
}

iscore::PluginControlInterface* iscore_plugin_scenario::make_control(
        iscore::Application& app)
{
    return new ScenarioControl{app};
}

QList<iscore::PanelFactory*> iscore_plugin_scenario::panels()
{
    return {
        new ProcessPanelFactory
    };
}

std::vector<iscore::FactoryListInterface*> iscore_plugin_scenario::factoryFamilies()
{
    return {new DynamicProcessList, new MoveEventList, new ScenarioContextMenuPluginList};
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_scenario::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        const auto& ctrls = ctx.components.controls();
        for(const auto& ctrl : ctrls)
        {
            if(auto scenario = dynamic_cast<ScenarioControl*>(ctrl))
                return {new ScenarioFactory{scenario->editionSettings()}};
        }
        return {};
    }

    if(factoryName == ScenarioActionsFactory::staticFactoryKey())
    {
        // new ScenarioCommonActionsFactory is instantiated in Control
        // because other plug ins need it.
        return {};
    }

    if(factoryName == MoveEventClassicFactory::staticFactoryKey())
    {
        return {new MoveEventClassicFactory};
    }

#if defined(ISCORE_LIB_INSPECTOR)
    if(factoryName == InspectorWidgetFactory::staticFactoryKey())
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
