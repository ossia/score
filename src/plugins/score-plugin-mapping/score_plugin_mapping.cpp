// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_mapping.hpp"

#include <Curve/Process/CurveProcessFactory.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <Mapping/Inspector/MappingInspectorFactory.hpp>
#include <Mapping/LocalTree.hpp>
#include <Mapping/MappingColors.hpp>
#include <Mapping/MappingExecution.hpp>
#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingPresenter.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Mapping/MappingView.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/ProcessFactory.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Mapping::LayerPresenter)
namespace Mapping
{

using MappingFactory = Process::ProcessFactory_T<Mapping::ProcessModel>;
using MappingLayerFactory = Curve::CurveLayerFactory_T<
    Mapping::ProcessModel,
    Mapping::LayerPresenter,
    Mapping::LayerView,
    Mapping::Colors,
    Process::DefaultHeaderDelegate>;
}

#include <score_plugin_mapping_commands_files.hpp>

score_plugin_mapping::score_plugin_mapping() = default;
score_plugin_mapping::~score_plugin_mapping() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_mapping::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Mapping;
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Mapping::MappingFactory>,
      FW<Process::LayerFactory, Mapping::MappingLayerFactory>,
      FW<Inspector::InspectorWidgetFactory, MappingInspectorFactory>,
      FW<LocalTree::ProcessComponentFactory, LocalTree::MappingComponentFactory>,
      FW<Execution::ProcessComponentFactory, Mapping::RecreateOnPlay::ComponentFactory>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_mapping::make_commands()
{
  using namespace Mapping;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      MappingCommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_mapping_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_mapping)
