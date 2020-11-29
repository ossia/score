// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_spline3d.hpp"

#include <Process/GenericProcessFactory.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>
#include <Spline3D/Execution.hpp>
#include <Spline3D/Model.hpp>
#include <Spline3D/Widget.hpp>
#include <Process/Dataflow/Port.hpp>
#include <wobjectimpl.h>
#include <score_plugin_spline3d_commands_files.hpp>
#include <Effect/EffectFactory.hpp>
#include <Control/DefaultEffectItem.hpp>
namespace Spline3D
{
using Factory = Process::ProcessFactory_T<Spline3D::ProcessModel>;
using LayerFactory = Process::EffectLayerFactory_T<
    Spline3D::ProcessModel,
    Process::DefaultEffectItem,
    Spline3D::Widget>;
}

score_plugin_spline3d::score_plugin_spline3d() = default;
score_plugin_spline3d::~score_plugin_spline3d() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_spline3d::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory,
         Spline3D::Factory>,
      FW<Process::LayerFactory,
         Spline3D::LayerFactory>,
      FW<Execution::ProcessComponentFactory,
         Spline3D::RecreateOnPlay::ComponentFactory>
      >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_spline3d::make_commands()
{
  using namespace Spline3D;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_spline3d_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_spline3d)
