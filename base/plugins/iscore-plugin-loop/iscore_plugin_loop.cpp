#include "iscore_plugin_loop.hpp"

#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopProcessFactory.hpp>


#include <Loop/Inspector/LoopInspectorFactory.hpp>
#include <Loop/Commands/EditScript.hpp>
#include <Loop/Commands/LoopCommandFactory.hpp>

iscore_plugin_loop::iscore_plugin_loop() :
    QObject {}
{
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_loop::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new LoopProcessFactory};
    }

    if(factoryName == InspectorWidgetFactory::staticFactoryKey())
    {
        return {new LoopInspectorFactory};
    }

    return {};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_loop::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{LoopCommandFactoryName(), CommandGeneratorMap{}};
    boost::mpl::for_each<
            boost::mpl::list<
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
