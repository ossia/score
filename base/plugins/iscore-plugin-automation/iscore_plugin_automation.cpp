#include <Automation/AutomationColors.hpp>
#include <Automation/AutomationLayerModel.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationPresenter.hpp>
#include <Automation/AutomationView.hpp>
#include <unordered_map>

#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include "iscore_plugin_automation.hpp"


#if defined(ISCORE_LIB_INSPECTOR)
#include <Automation/Inspector/AutomationInspectorFactory.hpp>
#include <Automation/Inspector/AutomationStateInspectorFactory.hpp>
#include <Automation/Inspector/CurvePointInspectorFactory.hpp>
#endif
#include <iscore_plugin_automation_commands_files.hpp>
#include <Curve/Process/CurveProcessFactory.hpp>
namespace Automation
{
DEFINE_CURVE_PROCESS_FACTORY(
        AutomationFactory,
        Automation::ProcessModel,
        Automation::LayerModel,
        Automation::LayerPresenter,
        Automation::LayerView,
        Automation::Colors)
}
iscore_plugin_automation::iscore_plugin_automation() :
    QObject {}
{
}


std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_automation::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
        FW<Process::ProcessFactory,
             Automation::AutomationFactory>,
#if defined(ISCORE_LIB_INSPECTOR)
        FW<Inspector::InspectorWidgetFactory,
             Automation::StateInspectorFactory,
             Automation::PointInspectorFactory>,
#endif
        FW<ProcessInspectorWidgetDelegateFactory,
             Automation::InspectorFactory>
    >>(ctx, key);
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_automation::make_commands()
{
    using namespace Automation;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{CommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_automation_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
