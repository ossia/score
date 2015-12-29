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

iscore_plugin_loop::iscore_plugin_loop() :
    QObject {}
{
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_loop::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& key) const
{
    if(key == Process::ProcessFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
            LoopProcessFactory>();
    }

    if(key == InspectorWidgetFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                LoopInspectorFactory>();
    }

    if(key == ConstraintInspectorDelegateFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                LoopConstraintInspectorDelegateFactory>();
    }

    if(key == TriggerCommandFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
            LoopTriggerCommandFactory
        >();
    }

    return {};
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
