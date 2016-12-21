#include "iscore_plugin_interpolation.hpp"
#include <Interpolation/Commands/CommandFactory.hpp>
#include <Interpolation/InterpolationFactory.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>

#include <iscore_plugin_interpolation_commands_files.hpp>

iscore_plugin_interpolation::iscore_plugin_interpolation() : QObject{}
{
}

iscore_plugin_interpolation::~iscore_plugin_interpolation()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_interpolation::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext, FW<Process::ProcessModelFactory, Interpolation::InterpolationFactory>, FW<Process::LayerFactory, Interpolation::InterpolationLayerFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_interpolation::make_commands()
{
  using namespace Interpolation;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Interpolation::CommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_interpolation_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}
