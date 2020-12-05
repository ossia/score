// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_spline.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <Spline/Execution.hpp>
#include <Spline/Model.hpp>
#include <Spline/Presenter.hpp>
#include <Spline/View.hpp>
#include <score_plugin_spline_commands_files.hpp>
#include <wobjectimpl.h>
namespace Spline
{
using SplineFactory = Process::ProcessFactory_T<Spline::ProcessModel>;
using SplineLayerFactory
    = Process::LayerFactory_T<Spline::ProcessModel, Spline::Presenter, Spline::View>;
}

score_plugin_spline::score_plugin_spline() = default;
score_plugin_spline::~score_plugin_spline() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_spline::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Spline::SplineFactory>,
      FW<Process::LayerFactory, Spline::SplineLayerFactory>,
      FW<Execution::ProcessComponentFactory, Spline::RecreateOnPlay::ComponentFactory>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_spline::make_commands()
{
  using namespace Spline;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_spline_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_spline)
