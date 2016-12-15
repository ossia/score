#include <Mapping/MappingColors.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingPresenter.hpp>
#include <Mapping/MappingView.hpp>

#include <iscore/tools/std/HashMap.hpp>

#include "iscore_plugin_mapping.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

#include <Curve/Process/CurveProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

#include <Mapping/Inspector/MappingInspectorFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
namespace Mapping
{

using MappingFactory
    = Process::GenericProcessModelFactory<Mapping::ProcessModel>;
using MappingLayerFactory = Curve::
    CurveLayerFactory_T<Mapping::ProcessModel, Mapping::Layer, Mapping::LayerPresenter, Mapping::LayerView, Mapping::Colors>;
}

#include <iscore_plugin_mapping_commands_files.hpp>

iscore_plugin_mapping::iscore_plugin_mapping() : QObject{}
{
}

iscore_plugin_mapping::~iscore_plugin_mapping()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_mapping::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  using namespace Mapping;
  return instantiate_factories<iscore::ApplicationContext, FW<Process::ProcessModelFactory, Mapping::MappingFactory>, FW<Process::LayerFactory, Mapping::MappingLayerFactory>, FW<Process::InspectorWidgetDelegateFactory, MappingInspectorFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_mapping::make_commands()
{
  using namespace Mapping;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      MappingCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_mapping_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}

iscore::Version iscore_plugin_mapping::version() const
{
  return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_mapping::key() const
{
  return_uuid("e097f02d-4676-492e-98b0-764963e1f792");
}
