#include "iscore_plugin_interpolation.hpp"
#include <Interpolation/InterpolationFactory.hpp>
#include <Interpolation/Commands/CommandFactory.hpp>

#include <iscore/plugins/customfactory/FactorySetup.hpp>

#include <iscore_plugin_interpolation_commands_files.hpp>

iscore_plugin_interpolation::iscore_plugin_interpolation() :
    QObject {}
{
}

iscore_plugin_interpolation::~iscore_plugin_interpolation()
{

}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_interpolation::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
                FW<Process::ProcessFactory, Interpolation::Factory>
            >>(ctx, key);
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_interpolation::make_commands()
{
    using namespace Interpolation;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{Interpolation::CommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
 #include <iscore_plugin_interpolation_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});


    return cmds;
}

iscore::Version iscore_plugin_interpolation::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_interpolation::key() const
{
    return_uuid("95fc1f7c-9ffd-4c2d-bb7f-bd43341dee8c");
}
