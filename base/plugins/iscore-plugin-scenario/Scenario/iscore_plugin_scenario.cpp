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
#include <State/Message.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <State/Value.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>

#include <Scenario/Inspector/Constraint/BaseConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorFactory.hpp>
#include <Scenario/Inspector/Constraint/ScenarioConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Interpolation/InterpolationInspectorWidget.hpp>
#include <Scenario/Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Scenario/Inspector/ScenarioInspectorWidgetFactoryWrapper.hpp>
#include <Scenario/iscore_plugin_scenario.hpp>
#include <Interpolation/InterpolationFactory.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <utility>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

#include <iscore_plugin_scenario_commands_files.hpp>

#include <State/Unit.hpp>
iscore_plugin_scenario::iscore_plugin_scenario()
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
  qRegisterMetaType<Scenario::ConstraintExecutionState>();
  qRegisterMetaType<QPointer<Process::LayerPresenter>>();

  qRegisterMetaType<Path<Scenario::ConstraintModel>>();
  qRegisterMetaType<Id<Process::ProcessModel>>();

  qRegisterMetaType<Scenario::OffsetBehavior>();
  qRegisterMetaTypeStreamOperators<Scenario::OffsetBehavior>();

  qRegisterMetaType<State::Unit>();
  qRegisterMetaTypeStreamOperators<State::Unit>();
  qRegisterMetaTypeStreamOperators<State::vec2f>();
  qRegisterMetaTypeStreamOperators<State::vec3f>();
  qRegisterMetaTypeStreamOperators<State::vec4f>();
}

iscore_plugin_scenario::~iscore_plugin_scenario() = default;

iscore::GUIApplicationPlugin*
iscore_plugin_scenario::make_guiApplicationPlugin(
    const iscore::GUIApplicationContext& app)
{
  using namespace Scenario;
  return new ScenarioApplicationPlugin{app};
}

std::vector<std::unique_ptr<iscore::InterfaceListBase>>
iscore_plugin_scenario::factoryFamilies()
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return make_ptr_vector<iscore::InterfaceListBase, Process::ProcessFactoryList, Process::StateProcessList, Process::LayerFactoryList, MoveEventList, CSPCoherencyCheckerList, ConstraintInspectorDelegateFactoryList, DisplayedElementsToolPaletteFactoryList, TriggerCommandFactoryList, DisplayedElementsProviderList, Process::InspectorWidgetDelegateFactoryList, Process::StateProcessInspectorWidgetDelegateFactoryList, DropHandlerList, ConstraintDropHandlerList>();
}

template <>
struct
    FactoryBuilder<iscore::GUIApplicationContext, Scenario::ScenarioTemporalLayerFactory>
{
  static auto make(const iscore::GUIApplicationContext& ctx)
  {
    using namespace Scenario;
    auto& appPlugin
        = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>();
    return std::make_unique<ScenarioTemporalLayerFactory>(
        appPlugin.editionSettings());
  }
};

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_scenario::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<iscore::ApplicationContext,
      FW<Process::ProcessModelFactory, ScenarioFactory, Interpolation::InterpolationFactory>,
      FW<Process::LayerFactory, Interpolation::InterpolationLayerFactory>,
      FW<MoveEventFactoryInterface, MoveEventClassicFactory>,
      FW<Process::InspectorWidgetDelegateFactory, ScenarioInspectorFactory, Interpolation::InspectorFactory>,
      FW<DisplayedElementsToolPaletteFactory, BaseScenarioDisplayedElementsToolPaletteFactory, ScenarioDisplayedElementsToolPaletteFactory>,
      FW<TriggerCommandFactory, ScenarioTriggerCommandFactory, BaseScenarioTriggerCommandFactory>,
      FW<DisplayedElementsProvider, ScenarioDisplayedElementsProvider, BaseScenarioDisplayedElementsProvider>,
      FW<iscore::DocumentDelegateFactory, Scenario::ScenarioDocumentFactory>,
      FW<iscore::SettingsDelegateFactory, Scenario::Settings::Factory>,
//      FW<iscore::PanelDelegateFactory, Scenario::PanelDelegateFactory>,
      FW<Scenario::DropHandler, Scenario::MessageDropHandler, Scenario::DropProcessInScenario>,
      FW<Scenario::ConstraintDropHandler, Scenario::DropProcessInConstraint, Scenario::AutomationDropHandler>,
      FW<Inspector::InspectorWidgetFactory, ScenarioInspectorWidgetFactoryWrapper, Interpolation::StateInspectorFactory>,
      FW<ConstraintInspectorDelegateFactory, ScenarioConstraintInspectorDelegateFactory, BaseConstraintInspectorDelegateFactory>,
      FW<iscore::ValidityChecker, ScenarioValidityChecker>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_scenario::make_commands()
{
  using namespace Scenario;
  using namespace Scenario::Command;
    using namespace Interpolation;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      ScenarioCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_scenario_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}


std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_scenario::guiFactories(
    const iscore::GUIApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<iscore::GUIApplicationContext,
      FW<Process::LayerFactory, ScenarioTemporalLayerFactory>>(
      ctx, key);
}
