#include <Loop/Commands/LoopCommandFactory.hpp>
#include <Loop/Inspector/LoopInspectorFactory.hpp>
#include <Loop/Inspector/LoopTriggerCommandFactory.hpp>
#include <Loop/LoopProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <string.h>
#include <iscore/tools/std/HashMap.hpp>

#include "iscore_plugin_loop.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Loop/LoopDisplayedElements.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_loop_commands_files.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>
iscore_plugin_loop::iscore_plugin_loop() : QObject{}
{
}

iscore_plugin_loop::~iscore_plugin_loop()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_loop::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<iscore::ApplicationContext, FW<Process::ProcessModelFactory, Loop::ProcessFactory>, FW<Process::LayerFactory, Loop::LayerFactory>, FW<Process::InspectorWidgetDelegateFactory, Loop::InspectorFactory>, FW<ConstraintInspectorDelegateFactory, Loop::ConstraintInspectorDelegateFactory>, FW<TriggerCommandFactory, LoopTriggerCommandFactory>, FW<Scenario::DisplayedElementsProvider, Loop::DisplayedElementsProvider>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_loop::make_commands()
{
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      LoopCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_loop_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}

iscore::Version iscore_plugin_loop::version() const
{
  return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_loop::key() const
{
  return_uuid("db40e6eb-add3-4b6d-8957-13690aec290b");
}
