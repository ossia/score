#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/BaseScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/ScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/DisplayedElements/BaseScenarioDisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/ScenarioDisplayedElementsProvider.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Panel/ProcessPanelFactory.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/iscore_plugin_scenario.hpp>
#include <State/Message.hpp>
#include <QMetaType>
#include <QString>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>
#include <Scenario/Application/Menus/Plugin/ScenarioContextMenuPluginList.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <State/Value.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>

namespace iscore {
class Application;
class DocumentDelegateFactoryInterface;
class FactoryListInterface;
class PanelFactory;
struct Address;
}  // namespace iscore

#if defined(ISCORE_LIB_INSPECTOR)
#include <Scenario/Inspector/Constraint/BaseConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Scenario/Inspector/Constraint/ScenarioConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Event/EventInspectorFactory.hpp>
#include <Scenario/Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Scenario/Inspector/State/StateInspectorFactory.hpp>
#include <Scenario/Inspector/TimeNode/TimeNodeInspectorFactory.hpp>
#endif

iscore_plugin_scenario::iscore_plugin_scenario() :
    QObject {},
        iscore::GUIApplicationContextPlugin_QtInterface {},
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
std::vector<iscore::DocumentDelegateFactoryInterface*> iscore_plugin_scenario::documents()
{
    return {new ScenarioDocument};
}

iscore::GUIApplicationContextPlugin* iscore_plugin_scenario::make_applicationPlugin(
        iscore::Application& app)
{
    return new ScenarioApplicationPlugin{app};
}

std::vector<iscore::PanelFactory*> iscore_plugin_scenario::panels()
{
    return {
        new ProcessPanelFactory
    };
}

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_scenario::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            DynamicProcessList,
            MoveEventList,
            ScenarioContextMenuPluginList,
            ConstraintInspectorDelegateFactoryList,
            DisplayedElementsToolPaletteFactoryList,
            TriggerCommandFactoryList,
            DisplayedElementsProviderList>();
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_scenario::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& key) const
{
    if(key == ProcessFactory::staticFactoryKey())
    {
        auto& appPlugin = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> vec;
        vec.emplace_back(new ScenarioFactory{appPlugin.editionSettings()});
        return vec;
    }

    if(key == ScenarioActionsFactory::staticFactoryKey())
    {
        // new ScenarioCommonActionsFactory is instantiated in ScenarioApplicationPlugin
        // because other plug ins need it.
        return {};
    }

    if(key == MoveEventClassicFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                MoveEventClassicFactory>();
    }

#if defined(ISCORE_LIB_INSPECTOR)
    if(key == InspectorWidgetFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
            ConstraintInspectorFactory,
            StateInspectorFactory,
            EventInspectorFactory,
            ScenarioInspectorFactory,
            TimeNodeInspectorFactory
        >();
    }

    if(key == ConstraintInspectorDelegateFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
            ScenarioConstraintInspectorDelegateFactory,
            BaseConstraintInspectorDelegateFactory
                >();
    }
#endif

    if(key == DisplayedElementsToolPaletteFactory::staticFactoryKey())
    {
    return make_ptr_vector<iscore::FactoryInterfaceBase,
            BaseScenarioDisplayedElementsToolPaletteFactory,
            ScenarioDisplayedElementsToolPaletteFactory
            >();
    }

    if(key == TriggerCommandFactory::staticFactoryKey())
    {
    return make_ptr_vector<iscore::FactoryInterfaceBase,
            ScenarioTriggerCommandFactory,
            BaseScenarioTriggerCommandFactory
            >();
    }

    if(key == DisplayedElementsProvider::staticFactoryKey())
    {
    return make_ptr_vector<iscore::FactoryInterfaceBase,
            ScenarioDisplayedElementsProvider,
            BaseScenarioDisplayedElementsProvider
            >();
    }
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
