// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <Scenario/Application/Drops/MessageDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/LoadPresetCommand.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/BaseScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/ScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/DisplayedElements/BaseScenarioDisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/ScenarioDisplayedElementsProvider.hpp>
#include <Scenario/Document/Event/ConditionView.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalHeader.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalHeader.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/Tempo/TempoFactory.hpp>
#include <Scenario/Document/Tempo/TempoInspector.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerList.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <score/graphics/GraphicsItem.hpp>
// #include <Scenario/Inspector/Interpolation/InterpolationInspectorWidget.hpp>
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <Scenario/Inspector/Interval/IntervalInspectorFactory.hpp>
#include <Scenario/Inspector/ObjectTree/ObjectItemModel.hpp>
#include <Scenario/Inspector/ScenarioInspectorFactory.hpp>
#include <Scenario/Inspector/ScenarioInspectorWidgetFactoryWrapper.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <Scenario/Process/ScenarioExecution.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Settings/ScenarioSettingsFactory.hpp>

#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/tools/std/HashMap.hpp>

#include <QList>
#include <QMetaType>
#include <QPainterPath>

#include <score_plugin_library.hpp>

// #include <Interpolation/InterpolationFactory.hpp>
#include <LocalTree/ScenarioComponent.hpp>

#include <score_plugin_scenario.hpp>
#include <score_plugin_scenario_commands_files.hpp>
#include <wobjectimpl.h>

#include <utility>

/*
class LoopProcessFactory final : public Process::ProcessModelFactory
{
public:

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return_uuid("995d41a8-0f10-4152-971d-e4c033579a02");
  }
  QString prettyName() const override
  {
    return Scenario::ScenarioFactory{}.prettyName();
  }
  QString category() const override
  {
    return Scenario::ScenarioFactory{}.category();
  }
  Process::Descriptor descriptor(QString str) const override
  {
    return Scenario::ScenarioFactory{}.descriptor(str);
  }
  Process::ProcessFlags flags() const override
  {
    return Scenario::ScenarioFactory{}.flags();
  }

  Scenario::ProcessModel* make(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx,
      QObject* parent) final override
  {
    SCORE_ABORT;
  }

  Scenario::ProcessModel* load(
      const VisitorVariant& vis,
      const score::DocumentContext& ctx,
      QObject* parent) final override
  {
    if(vis.identifier != JSONObject::type())
    {
      SCORE_ABORT;
    }
    auto& v = static_cast<JSONObject::Deserializer&>(vis.visitor);

    return new Scenario::ProcessModel{v, ctx, parent};
  }
};
*/

// W_OBJECT_IMPL(Interpolation::Presenter)
score_plugin_scenario::score_plugin_scenario()
{
  using namespace Scenario;
  // QMetaType::registerComparators<ossia::value>();
  // QMetaType::registerComparators<State::Message>();
  // QMetaType::registerComparators<State::MessageList>();

  qRegisterMetaType<State::Expression>();
  qRegisterMetaType<ExecutionStatus>();
  qRegisterMetaType<Scenario::IntervalExecutionState>();
  qRegisterMetaType<QPointer<Process::LayerPresenter>>();

  qRegisterMetaType<Path<Scenario::IntervalModel>>();
  qRegisterMetaType<Id<Process::ProcessModel>>();

  qRegisterMetaType<LockMode>();
  qRegisterMetaType<Scenario::OffsetBehavior>();

  qRegisterMetaType<State::Unit>();

  qRegisterMetaType<std::shared_ptr<Execution::ProcessComponent>>();
  qRegisterMetaType<std::shared_ptr<Execution::EventComponent>>();
  qRegisterMetaType<ossia::time_event::status>();
  qRegisterMetaType<ossia::time_value>();

  ::registerItemHelp(
      Scenario::TimeSyncView::Type, "",
      QUrl("https://ossia.io/score-docs/processes/scenario.html#triggers"));
  ::registerItemHelp(
      Scenario::TriggerView::Type, "",
      QUrl("https://ossia.io/score-docs/processes/scenario.html#triggers"));
  ::registerItemHelp(
      Scenario::ConditionView::Type, "",
      QUrl("https://ossia.io/score-docs/processes/scenario.html#conditions"));
  ::registerItemHelp(
      Scenario::EventView::Type, "",
      QUrl("https://ossia.io/score-docs/processes/scenario.html#conditions"));
  ::registerItemHelp(
      Scenario::StateView::Type, "",
      QUrl("https://ossia.io/score-docs/processes/scenario.html#states"));
  ::registerItemHelp(
      Scenario::IntervalView::Type, "",
      QUrl("https://ossia.io/score-docs/processes/scenario.html#intervals"));
  ::registerItemHelp(
      Scenario::ScenarioView::Type, "",
      QUrl("https://ossia.io/score-docs/processes/scenario.html"));
}

score_plugin_scenario::~score_plugin_scenario() = default;

score::GUIApplicationPlugin*
score_plugin_scenario::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  using namespace Scenario;
  return new ScenarioApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_scenario::factoryFamilies()
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return make_ptr_vector<
      score::InterfaceListBase, MoveEventList, CSPCoherencyCheckerList,
      DisplayedElementsToolPaletteFactoryList, TriggerCommandFactoryList,
      DisplayedElementsProviderList, DropHandlerList, IntervalDropHandlerList,
      IntervalResizerList>();
}

template <>
struct FactoryBuilder<
    score::GUIApplicationContext, Scenario::ScenarioTemporalLayerFactory>
{
  static auto make(const score::GUIApplicationContext& ctx)
  {
    using namespace Scenario;
    auto& appPlugin = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>();
    return new ScenarioTemporalLayerFactory(appPlugin.editionSettings());
  }
};

std::vector<score::InterfaceBase*> score_plugin_scenario::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, ScenarioFactory, Scenario::TempoFactory
         //, LoopProcessFactory
         //       , Interpolation::InterpolationFactory
         >,
      FW<Process::LayerFactory,
         //         Interpolation::InterpolationLayerFactory,
         Scenario::TempoLayerFactory>,
      FW<MoveEventFactoryInterface, MoveEventClassicFactory>,
      FW<DisplayedElementsToolPaletteFactory,
         BaseScenarioDisplayedElementsToolPaletteFactory,
         ScenarioDisplayedElementsToolPaletteFactory>,
      FW<TriggerCommandFactory, ScenarioTriggerCommandFactory,
         BaseScenarioTriggerCommandFactory>,
      FW<Process::LoadPresetCommandFactory, Scenario::Command::LoadPresetCommandFactory>,
      FW<DisplayedElementsProvider, DefaultDisplayedElementsProvider,
         ScenarioDisplayedElementsProvider, BaseScenarioDisplayedElementsProvider>,
      FW<score::DocumentDelegateFactory, Scenario::ScenarioDocumentFactory>,
      FW<score::SettingsDelegateFactory, Scenario::Settings::Factory>,
      FW<score::PanelDelegateFactory, Scenario::ObjectPanelDelegateFactory>,
      //      FW<score::PanelDelegateFactory, Scenario::PanelDelegateFactory>,
      FW<Process::ProcessDropHandler, Scenario::ProcessDataDropHandler>,
      FW<Scenario::DropHandler, Scenario::MessageDropHandler, Scenario::DropScenario,
         Scenario::DropScoreInScenario, Scenario::DropProcessInScenario,
         Scenario::DropPresetInScenario, Scenario::DropLayerInScenario>,
      FW<Scenario::IntervalDropHandler, Scenario::DropProcessInInterval,
         Scenario::DropLayerInInterval, Scenario::DropScoreInInterval,
         Scenario::AutomationDropHandler, Scenario::DropPresetInInterval>,
      FW<Inspector::InspectorWidgetFactory, ScenarioInspectorWidgetFactoryWrapper,
         Scenario::TempoPointInspectorFactory, Scenario::InspectorWidgetDelegateFactory
         //          , Interpolation::StateInspectorFactory
         //          , Interpolation::InspectorFactory
         >,
      FW<score::ValidityChecker, ScenarioValidityChecker>,

      FW<LocalTree::ProcessComponentFactory, LocalTree::ScenarioComponentFactory>,
      FW<Execution::ProcessComponentFactory, Execution::ScenarioComponentFactory>,
      FW<Library::LibraryInterface, Scenario::SlotLibraryHandler,
         Scenario::ScenarioLibraryHandler>,
      FW<Scenario::IntervalResizer, Scenario::ScenarioIntervalResizer,
         Scenario::BaseScenarioIntervalResizer>,
      FW<score::ObjectEditor, Scenario::ScenarioEditor>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_scenario::make_commands()
{
  using namespace Scenario;
  using namespace Dataflow;
  using namespace Scenario::Command;
  //using namespace Interpolation;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_scenario_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

std::vector<score::InterfaceBase*> score_plugin_scenario::guiFactories(
    const score::GUIApplicationContext& ctx, const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<
      score::GUIApplicationContext,
      FW<Process::LayerFactory, ScenarioTemporalLayerFactory>>(ctx, key);
}

std::vector<score::PluginKey> score_plugin_scenario::required() const
{
  return {score_plugin_library::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_scenario)
