#include <QString>
#include <unordered_map>
#include <iscore/tools/ForEachType.hpp>
#include "Commands/IScoreCohesionCommandFactory.hpp"
#include "IScoreCohesionApplicationPlugin.hpp"
#include "iscore_plugin_cohesion.hpp"
#include <iscore_plugin_cohesion_commands_files.hpp>

namespace iscore {

}  // namespace iscore


iscore_plugin_cohesion::iscore_plugin_cohesion() :
    QObject {}
{
}

iscore_plugin_cohesion::~iscore_plugin_cohesion()
{

}

iscore::GUIApplicationContextPlugin* iscore_plugin_cohesion::make_applicationPlugin(
        const iscore::ApplicationContext& app)
{
    return new IScoreCohesionApplicationPlugin {app};
}

QStringList iscore_plugin_cohesion::required() const
{
    return {"Scenario"};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_cohesion::make_commands()
{
    using namespace Recording;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{IScoreCohesionCommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_cohesion_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}

iscore::Version iscore_plugin_cohesion::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_cohesion::key() const
{
    return "659ba25e-97e5-40d9-8db8-f7a8537035ad";
}
