// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <JS/Executor/Component.hpp>
#include <JS/Executor/StateComponent.hpp>
#include <JS/Inspector/JSInspectorFactory.hpp>
#include <JS/JSProcessFactory.hpp>
#include <JS/JSStateProcess.hpp>
#include <iscore/tools/std/HashMap.hpp>

#include "JS/Commands/JSCommandFactory.hpp"
#include "iscore_plugin_js.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_plugin_js_commands_files.hpp>

iscore_plugin_js::iscore_plugin_js() : QObject{}
{
}

iscore_plugin_js::~iscore_plugin_js()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_js::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext,
      FW<Process::ProcessModelFactory, JS::ProcessFactory>,
      FW<Process::LayerFactory, JS::LayerFactory>,
      FW<Process::StateProcessFactory, JS::StateProcessFactory>,
      FW<Process::InspectorWidgetDelegateFactory, JS::InspectorFactory>,
      FW<Process::StateProcessInspectorWidgetDelegateFactory, JS::StateInspectorFactory>,
      FW<Engine::Execution::ProcessComponentFactory, JS::Executor::ComponentFactory>,
      FW<Engine::Execution::StateProcessComponentFactory, JS::Executor::StateProcessComponentFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_js::make_commands()
{
  using namespace JS;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      JS::CommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_js_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}
