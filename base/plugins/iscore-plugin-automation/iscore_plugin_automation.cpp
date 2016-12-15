#include <Automation/AutomationColors.hpp>
#include <Automation/AutomationLayerModel.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationPresenter.hpp>
#include <Automation/AutomationView.hpp>
#include <iscore/tools/std/HashMap.hpp>

#include "iscore_plugin_automation.hpp"
#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

#include <Automation/Inspector/AutomationInspectorFactory.hpp>
#include <Automation/Inspector/AutomationStateInspectorFactory.hpp>
#include <Automation/Inspector/CurvePointInspectorFactory.hpp>
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <iscore_plugin_automation_commands_files.hpp>
namespace Automation
{
using AutomationFactory
    = Process::GenericProcessModelFactory<Automation::ProcessModel>;
using AutomationLayerFactory = Curve::
    CurveLayerFactory_T<Automation::ProcessModel, Automation::Layer, Automation::LayerPresenter, Automation::LayerView, Automation::Colors>;
}

iscore_plugin_automation::iscore_plugin_automation() = default;
iscore_plugin_automation::~iscore_plugin_automation() = default;

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_automation::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext,
      FW<Process::ProcessModelFactory, Automation::AutomationFactory>, FW<Process::LayerFactory, Automation::AutomationLayerFactory>, FW<Inspector::InspectorWidgetFactory, Automation::StateInspectorFactory, Automation::PointInspectorFactory>, FW<Process::InspectorWidgetDelegateFactory, Automation::InspectorFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_automation::make_commands()
{
  using namespace Automation;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_automation_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}

iscore::Version iscore_plugin_automation::version() const
{
  return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_automation::key() const
{
  return_uuid("255cbc40-c7e9-4bb2-87ea-8ad803fb9f2b");
}
