#include <Loop/Commands/LoopCommandFactory.hpp>
#include <Loop/Inspector/LoopInspectorFactory.hpp>
#include <Loop/Inspector/LoopTriggerCommandFactory.hpp>
#include <Loop/LoopProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <boost/optional/optional.hpp>
#include <string.h>
#include <unordered_map>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include "iscore_plugin_loop.hpp"
#include <iscore_plugin_loop_commands_files.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>
iscore_plugin_loop::iscore_plugin_loop() :
    QObject {}
{
}

iscore_plugin_loop::~iscore_plugin_loop()
{

}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_loop::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    using namespace Scenario;
    using namespace Scenario::Command;
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
            FW<Process::ProcessFactory,
                Loop::ProcessFactory>,
            FW<Process::InspectorWidgetDelegateFactory,
                Loop::InspectorFactory>,
            FW<ConstraintInspectorDelegateFactory,
                Loop::ConstraintInspectorDelegateFactory>,
            FW<TriggerCommandFactory,
                LoopTriggerCommandFactory>
            >
            >(ctx, key);
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_loop::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{LoopCommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_loop_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});


    return cmds;
}

iscore::Version iscore_plugin_loop::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_loop::key() const
{
    return "db40e6eb-add3-4b6d-8957-13690aec290b";
}
