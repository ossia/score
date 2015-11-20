#include "iscore_plugin_automation.hpp"

#include <Curve/Process/CurveProcessFactory.hpp>

#include <Automation/AutomationColors.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationView.hpp>
#include <Automation/AutomationLayerModel.hpp>
#include <Automation/AutomationPresenter.hpp>

#if defined(ISCORE_LIB_INSPECTOR)
#include "Automation/Inspector/AutomationInspectorFactory.hpp"
#include "Automation/Inspector/CurvePointInspectorFactory.hpp"
#include "Automation/Inspector/AutomationStateInspectorFactory.hpp"
#endif

#include <iscore_plugin_automation_commands_files.hpp>

DEFINE_CURVE_PROCESS_FACTORY(
        AutomationFactory,
        AutomationProcessMetadata,
        AutomationModel,
        AutomationLayerModel,
        AutomationPresenter,
        AutomationView,
        AutomationColors)

iscore_plugin_automation::iscore_plugin_automation() :
    QObject {}
{
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_automation::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new AutomationFactory};
    }

#if defined(ISCORE_LIB_INSPECTOR)
    if(factoryName == InspectorWidgetFactory::staticFactoryKey())
    {
        return {new AutomationInspectorFactory,
                new AutomationStateInspectorFactory,
                new CurvePointInspectorFactory};
    }
#endif
    return {};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_automation::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{AutomationCommandFactoryName(), CommandGeneratorMap{}};

    using Types = iscore::commands::TypeList<
  #include <iscore_plugin_automation_commands.hpp>
      >;
    iscore::commands::ForEach<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
