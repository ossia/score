// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Mapping/MappingColors.hpp>
#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingPresenter.hpp>
#include <Mapping/MappingView.hpp>

#include <score/tools/std/HashMap.hpp>

#include "score_plugin_mapping.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Process/ProcessFactory.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

#include <Curve/Process/CurveProcessFactory.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>

#include <Mapping/Inspector/MappingInspectorFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
namespace Mapping
{

using MappingFactory
    = Process::ProcessFactory_T<Mapping::ProcessModel>;
using MappingLayerFactory = Curve::
    CurveLayerFactory_T<Mapping::ProcessModel, Mapping::LayerPresenter, Mapping::LayerView, Mapping::Colors>;
}

#include <score_plugin_mapping_commands_files.hpp>

score_plugin_mapping::score_plugin_mapping() : QObject{}
{
}

score_plugin_mapping::~score_plugin_mapping()
{
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_mapping::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Mapping;
  return instantiate_factories<score::ApplicationContext
      , FW<Process::ProcessModelFactory, Mapping::MappingFactory>
      , FW<Process::LayerFactory, Mapping::MappingLayerFactory>
      , FW<Inspector::InspectorWidgetFactory, MappingInspectorFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_mapping::make_commands()
{
  using namespace Mapping;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      MappingCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <score_plugin_mapping_commands.hpp>
      >;
  for_each_type<Types>(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
