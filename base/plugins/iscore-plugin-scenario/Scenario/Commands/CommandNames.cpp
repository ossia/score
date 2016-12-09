#include <Scenario/iscore_plugin_scenario.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <utility>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <iscore_plugin_scenario_commands_files.hpp>

std::pair<const CommandParentFactoryKey, CommandGeneratorMap>
iscore_plugin_scenario::make_commands()
{
  using namespace Scenario;
  using namespace Scenario::Command;
  std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{
      ScenarioCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_scenario_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}
