// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QMetaType>
#include <QString>
#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <Scenario/Application/Drops/MessageDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/BaseScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/ScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
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
#include <Scenario/score_plugin_scenario.hpp>
#include <State/Message.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <State/Value.hpp>

#include <score/plugins/customfactory/FactorySetup.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>

#include <Scenario/Inspector/Interval/BaseIntervalInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorFactory.hpp>
#include <Scenario/Inspector/Interval/ScenarioIntervalInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Interpolation/InterpolationInspectorWidget.hpp>
#include <Scenario/Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Scenario/Inspector/ScenarioInspectorWidgetFactoryWrapper.hpp>
#include <Scenario/score_plugin_scenario.hpp>
#include <Interpolation/InterpolationFactory.hpp>
#include <score/tools/std/HashMap.hpp>
#include <utility>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/command/Command.hpp>
#include <score/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <Scenario/Inspector/ObjectTree/ObjectItemModel.hpp>
#include <Dataflow/Inspector/DataflowWidget.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Effect/EffectFactory.hpp>
#include <State/ValueSerialization.hpp>

#include <score_plugin_scenario_commands_files.hpp>

#include <State/Unit.hpp>
#include <QPainterPath>
#include <QList>
#include <score/widgets/GraphicsItem.hpp>

score_plugin_scenario::score_plugin_scenario()
{
  using namespace Scenario;
  // QMetaType::registerComparators<ossia::value>();
  QMetaType::registerComparators<State::Message>();
  QMetaType::registerComparators<State::MessageList>();

  qRegisterMetaType<State::Expression>();
  qRegisterMetaTypeStreamOperators<State::Message>();
  qRegisterMetaTypeStreamOperators<State::MessageList>();
  qRegisterMetaTypeStreamOperators<State::Address>();
  qRegisterMetaTypeStreamOperators<ossia::value>();
  qRegisterMetaTypeStreamOperators<State::Expression>();

  qRegisterMetaTypeStreamOperators<TimeVal>();
  qRegisterMetaType<ExecutionStatus>();
  qRegisterMetaType<Scenario::IntervalExecutionState>();
  qRegisterMetaType<QPointer<Process::LayerPresenter>>();

  qRegisterMetaType<Path<Scenario::IntervalModel>>();
  qRegisterMetaType<Id<Process::ProcessModel>>();

  qRegisterMetaType<LockMode>();
  qRegisterMetaType<Scenario::OffsetBehavior>();
  qRegisterMetaTypeStreamOperators<Scenario::OffsetBehavior>();

  qRegisterMetaType<State::Unit>();
  qRegisterMetaTypeStreamOperators<State::Unit>();
  qRegisterMetaTypeStreamOperators<State::vec2f>();
  qRegisterMetaTypeStreamOperators<State::vec3f>();
  qRegisterMetaTypeStreamOperators<State::vec4f>();

  qRegisterMetaType<QPainterPath>();
  qRegisterMetaType<QList<QPainterPath>>();
}

score_plugin_scenario::~score_plugin_scenario() = default;

score::GUIApplicationPlugin*
score_plugin_scenario::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  using namespace Scenario;
  return new ScenarioApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_scenario::factoryFamilies()
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return make_ptr_vector<score::InterfaceListBase,
      Process::ProcessFactoryList,
      Process::PortFactoryList,
      Process::LayerFactoryList,
      Process::ProcessFactoryList,
      MoveEventList,
      CSPCoherencyCheckerList,
      IntervalInspectorDelegateFactoryList,
      DisplayedElementsToolPaletteFactoryList,
      TriggerCommandFactoryList,
      DisplayedElementsProviderList,
      DropHandlerList,
      IntervalDropHandlerList>();
}

template <>
struct
    FactoryBuilder<score::GUIApplicationContext, Scenario::ScenarioTemporalLayerFactory>
{
  static auto make(const score::GUIApplicationContext& ctx)
  {
    using namespace Scenario;
    auto& appPlugin
        = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>();
    return std::make_unique<ScenarioTemporalLayerFactory>(
        appPlugin.editionSettings());
  }
};

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_scenario::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<score::ApplicationContext,
      FW<Process::ProcessModelFactory, ScenarioFactory, Interpolation::InterpolationFactory>,
      FW<Process::LayerFactory, Interpolation::InterpolationLayerFactory>,
      FW<MoveEventFactoryInterface, MoveEventClassicFactory>,
      FW<DisplayedElementsToolPaletteFactory, BaseScenarioDisplayedElementsToolPaletteFactory, ScenarioDisplayedElementsToolPaletteFactory>,
      FW<TriggerCommandFactory, ScenarioTriggerCommandFactory, BaseScenarioTriggerCommandFactory>,
      FW<DisplayedElementsProvider, ScenarioDisplayedElementsProvider, BaseScenarioDisplayedElementsProvider>,
      FW<score::DocumentDelegateFactory, Scenario::ScenarioDocumentFactory>,
      FW<score::SettingsDelegateFactory, Scenario::Settings::Factory>,
      FW<score::PanelDelegateFactory, Scenario::ObjectPanelDelegateFactory>,
//      FW<score::PanelDelegateFactory, Scenario::PanelDelegateFactory>,
      FW<Scenario::DropHandler, Scenario::MessageDropHandler, Scenario::DropProcessInScenario, Scenario::DropPortInScenario>,
      FW<Scenario::IntervalDropHandler, Scenario::DropProcessInInterval, Scenario::AutomationDropHandler>,
      FW<Inspector::InspectorWidgetFactory, ScenarioInspectorWidgetFactoryWrapper, Interpolation::StateInspectorFactory, ScenarioInspectorFactory, Interpolation::InspectorFactory, Dataflow::CableInspectorFactory>,
      FW<IntervalInspectorDelegateFactory, ScenarioIntervalInspectorDelegateFactory, BaseIntervalInspectorDelegateFactory>,
      FW<score::ValidityChecker, ScenarioValidityChecker>,
      FW<Process::PortFactory, Process::InletFactory, Process::ControlInletFactory, Process::OutletFactory, Process::ControlOutletFactory>
      //, FW<Dataflow::ProcessComponentFactory, Dataflow::ScenarioComponentFactory>
      >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_scenario::make_commands()
{
  using namespace Scenario;
  using namespace Dataflow;
  using namespace Scenario::Command;
    using namespace Interpolation;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      ScenarioCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <score_plugin_scenario_commands.hpp>
      >;
  for_each_type<Types>(score::commands::FactoryInserter{cmds.second});

  return cmds;
}


std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_scenario::guiFactories(
    const score::GUIApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<score::GUIApplicationContext,
      FW<Process::LayerFactory, ScenarioTemporalLayerFactory>>(
      ctx, key);
}
