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
#include <Process/LayerPresenter.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>
#include <Scenario/Application/Menus/Plugin/ScenarioContextMenuPluginList.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <State/Value.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>

namespace iscore {

class DocumentDelegateFactoryInterface;
class FactoryListInterface;
class PanelFactory;
}
namespace State
{
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
    using namespace Scenario;
    QMetaType::registerComparators<State::Value>();
    QMetaType::registerComparators<State::Message>();
    QMetaType::registerComparators<State::MessageList>();

    qRegisterMetaTypeStreamOperators<State::Message>();
    qRegisterMetaTypeStreamOperators<State::MessageList>();
    qRegisterMetaTypeStreamOperators<State::Address>();
    qRegisterMetaTypeStreamOperators<State::Value>();
    qRegisterMetaTypeStreamOperators<State::ValueList>();

    qRegisterMetaTypeStreamOperators<TimeValue>();
    qRegisterMetaType<ExecutionStatus>();
    qRegisterMetaType<QPointer<Process::LayerPresenter>>();
}

// Interfaces implementations :
std::vector<iscore::DocumentDelegateFactoryInterface*> iscore_plugin_scenario::documents()
{
    using namespace Scenario;
    return {new ScenarioDocumentFactory};
}

iscore::GUIApplicationContextPlugin* iscore_plugin_scenario::make_applicationPlugin(
        const iscore::ApplicationContext& app)
{
    using namespace Scenario;
    return new ScenarioApplicationPlugin{app};
}

std::vector<iscore::PanelFactory*> iscore_plugin_scenario::panels()
{
    return {
        new Scenario::ProcessPanelFactory
    };
}

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_scenario::factoryFamilies()
{
    using namespace Scenario;
    using namespace Scenario::Command;
    return make_ptr_vector<iscore::FactoryListInterface,
            Process::ProcessList,
            Process::StateProcessList,
            MoveEventList,
            ScenarioContextMenuPluginList,
            ConstraintInspectorDelegateFactoryList,
            DisplayedElementsToolPaletteFactoryList,
            TriggerCommandFactoryList,
            DisplayedElementsProviderList,
            ProcessInspectorWidgetDelegateFactoryList>();
}

template<>
struct FactoryBuilder<iscore::ApplicationContext, Scenario::ScenarioFactory>
{
        static auto make(const iscore::ApplicationContext& ctx)
        {
            using namespace Scenario;
            auto& appPlugin = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
            return std::make_unique<ScenarioFactory>(appPlugin.editionSettings());
        }
};

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_scenario::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    using namespace Scenario;
    using namespace Scenario::Command;
    /*
        if(key == ScenarioActionsFactory::static_abstractFactoryKey())
        {
            // new ScenarioCommonActionsFactory is instantiated in ScenarioApplicationPlugin
            // because other plug ins need it.
            return {};
        }
    */

    // TODO use me everywhere.
    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
    FW<Process::ProcessFactory,
        ScenarioFactory>,
    FW<MoveEventFactoryInterface,
        MoveEventClassicFactory>,
    FW<ProcessInspectorWidgetDelegateFactory,
        ScenarioInspectorFactory>,
    FW<DisplayedElementsToolPaletteFactory,
        BaseScenarioDisplayedElementsToolPaletteFactory,
        ScenarioDisplayedElementsToolPaletteFactory>,
    FW<TriggerCommandFactory,
        ScenarioTriggerCommandFactory,
        BaseScenarioTriggerCommandFactory>,
    FW<DisplayedElementsProvider,
        ScenarioDisplayedElementsProvider,
        BaseScenarioDisplayedElementsProvider>
#if defined(ISCORE_LIB_INSPECTOR)
    ,
    FW<Inspector::InspectorWidgetFactory,
        ConstraintInspectorFactory,
        StateInspectorFactory,
        EventInspectorFactory,
        TimeNodeInspectorFactory>,
    FW<ConstraintInspectorDelegateFactory,
        ScenarioConstraintInspectorDelegateFactory,
        BaseConstraintInspectorDelegateFactory>
#endif
    >>(ctx, key);
}


QStringList iscore_plugin_scenario::required() const
{
    return {};
}

QStringList iscore_plugin_scenario::offered() const
{
    return {"Scenario"};
}
