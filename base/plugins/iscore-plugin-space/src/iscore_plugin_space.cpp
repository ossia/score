#include "iscore_plugin_space.hpp"

#include "SpaceProcessFactory.hpp"
#include "SpaceApplicationPlugin.hpp"
#include "Area/AreaFactory.hpp"
#include "Area/SingletonAreaFactoryList.hpp"

#include "Area/Circle/CircleAreaFactory.hpp"
#include "Area/Pointer/PointerAreaFactory.hpp"
#include "Area/Generic/GenericAreaFactory.hpp"
#include <src/SpaceProcess.hpp>
#include <src/LocalTree/GenericAreaComponentFactory.hpp>
#include <src/LocalTree/GenericComputationComponentFactory.hpp>
#include <src/LocalTree/ProcessComponentFactory.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_plugin_space::iscore_plugin_space() :
    QObject {}
{
}

iscore_plugin_space::~iscore_plugin_space()
{

}


std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_space::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& key) const
{

    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
        FW<Process::ProcessFactory,
            Space::ProcessFactory>,
        FW<Space::AreaFactory,
            Space::GenericAreaFactory,
            Space::CircleAreaFactory,
            Space::PointerAreaFactory>,
        FW<Ossia::LocalTree::ProcessComponentFactory,
            Space::LocalTree::ProcessLocalTreeFactory>,
        FW<Space::LocalTree::AreaComponentFactory,
            Space::LocalTree::GenericAreaComponentFactory // Shall be last in the vector so must be first here, because of the recursion order of C++ templates in instantiate_factories
            >,
        FW<Space::LocalTree::ComputationComponentFactory,
            Space::LocalTree::GenericComputationComponentFactory>
      >
     >(ctx, key);
}


std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_space::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Space::SingletonAreaFactoryList,
            Space::LocalTree::AreaComponentFactoryList,
            Space::LocalTree::ComputationComponentFactoryList
            >();
}


#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <src/Commands/SpaceCommandFactory.hpp>

#include <iscore_plugin_space_commands_files.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>

std::pair<const CommandParentFactoryKey, CommandGeneratorMap>
iscore_plugin_space::make_commands()
{
    using namespace Space;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{
        SpaceCommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_space_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
