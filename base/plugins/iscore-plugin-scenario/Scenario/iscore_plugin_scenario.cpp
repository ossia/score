#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioValidity.hpp>
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
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerList.hpp>
#include <Scenario/Panel/PanelDelegateFactory.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Settings/ScenarioSettingsFactory.hpp>
#include <Scenario/iscore_plugin_scenario.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Drops/MessageDropHandler.hpp>
#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <State/Message.hpp>
#include <QMetaType>
#include <QString>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <State/Value.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>

#include <Scenario/Inspector/Constraint/BaseConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Scenario/Inspector/Constraint/ScenarioConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Scenario/Inspector/ScenarioInspectorWidgetFactoryWrapper.hpp>
#include <Scenario/Inspector/Interpolation/InterpolationInspectorWidget.hpp>

iscore_plugin_scenario::iscore_plugin_scenario()
{
    using namespace Scenario;
    //QMetaType::registerComparators<State::Value>();
    QMetaType::registerComparators<State::Message>();
    QMetaType::registerComparators<State::MessageList>();

    qRegisterMetaTypeStreamOperators<State::Message>();
    qRegisterMetaTypeStreamOperators<State::MessageList>();
    qRegisterMetaTypeStreamOperators<State::Address>();
    qRegisterMetaTypeStreamOperators<State::Value>();
    qRegisterMetaTypeStreamOperators<State::ValueList>();

    qRegisterMetaTypeStreamOperators<TimeValue>();
    qRegisterMetaType<ExecutionStatus>();
    qRegisterMetaType<Scenario::ConstraintExecutionState>();
    qRegisterMetaType<QPointer<Process::LayerPresenter>>();

    qRegisterMetaType<Path<Scenario::ConstraintModel>>();
    qRegisterMetaType<Id<Process::ProcessModel>>();

    qRegisterMetaType<Scenario::OffsetBehavior>();
    qRegisterMetaTypeStreamOperators<Scenario::OffsetBehavior>();
}

iscore_plugin_scenario::~iscore_plugin_scenario() = default;

iscore::GUIApplicationContextPlugin* iscore_plugin_scenario::make_applicationPlugin(
        const iscore::GUIApplicationContext& app)
{
    using namespace Scenario;
    return new ScenarioApplicationPlugin{app};
}

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_scenario::factoryFamilies()
{
    using namespace Scenario;
    using namespace Scenario::Command;
    return make_ptr_vector<iscore::FactoryListInterface,
            Process::ProcessFactoryList,
            Process::StateProcessList,
            Process::LayerFactoryList,
            MoveEventList,
            CSPCoherencyCheckerList,
            ConstraintInspectorDelegateFactoryList,
            DisplayedElementsToolPaletteFactoryList,
            TriggerCommandFactoryList,
            DisplayedElementsProviderList,
            Process::InspectorWidgetDelegateFactoryList,
            Process::StateProcessInspectorWidgetDelegateFactoryList,
            DropHandlerList,
            ConstraintDropHandlerList
            >();
}

template<>
struct FactoryBuilder<iscore::ApplicationContext, Scenario::ScenarioTemporalLayerFactory>
{
        static auto make(const iscore::ApplicationContext& ctx)
        {
            using namespace Scenario;
            auto& appPlugin = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
            return std::make_unique<ScenarioTemporalLayerFactory>(appPlugin.editionSettings());
        }
};

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_scenario::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    using namespace Scenario;
    using namespace Scenario::Command;
    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
    FW<Process::ProcessModelFactory,
        ScenarioFactory>,
   FW<Process::LayerFactory,
        ScenarioTemporalLayerFactory>,
    FW<MoveEventFactoryInterface,
        MoveEventClassicFactory>,
    FW<Process::InspectorWidgetDelegateFactory,
        ScenarioInspectorFactory,
        Interpolation::InspectorFactory>,
    FW<DisplayedElementsToolPaletteFactory,
        BaseScenarioDisplayedElementsToolPaletteFactory,
        ScenarioDisplayedElementsToolPaletteFactory>,
    FW<TriggerCommandFactory,
        ScenarioTriggerCommandFactory,
        BaseScenarioTriggerCommandFactory>,
    FW<DisplayedElementsProvider,
        ScenarioDisplayedElementsProvider,
        BaseScenarioDisplayedElementsProvider>,
    FW<iscore::DocumentDelegateFactory,
        Scenario::ScenarioDocumentFactory>,
    FW<iscore::SettingsDelegateFactory,
        Scenario::Settings::Factory>,
    FW<iscore::PanelDelegateFactory,
        Scenario::PanelDelegateFactory>,
    FW<Scenario::DropHandler,
        Scenario::MessageDropHandler>,
    FW<Scenario::ConstraintDropHandler,
        Scenario::AutomationDropHandler>,
    FW<Inspector::InspectorWidgetFactory,
        ScenarioInspectorWidgetFactoryWrapper,
        Interpolation::StateInspectorFactory>,
    FW<ConstraintInspectorDelegateFactory,
        ScenarioConstraintInspectorDelegateFactory,
        BaseConstraintInspectorDelegateFactory>,
    FW<iscore::ValidityChecker,
        ScenarioValidityChecker>
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

iscore::Version iscore_plugin_scenario::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_scenario::key() const
{
    return_uuid("8439ef6c-90c3-4e08-8185-6a0f3c87f8b4");
}
