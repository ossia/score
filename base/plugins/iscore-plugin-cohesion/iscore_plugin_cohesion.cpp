#include <QString>
#include <unordered_map>

#include "Commands/IScoreCohesionCommandFactory.hpp"
#include "IScoreCohesionApplicationPlugin.hpp"
#include "iscore_plugin_cohesion.hpp"
#include <iscore_plugin_cohesion_commands_files.hpp>

namespace iscore {
class Application;
}  // namespace iscore


iscore_plugin_cohesion::iscore_plugin_cohesion() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin* iscore_plugin_cohesion::make_applicationPlugin(
        iscore::Application& app)
{
    return new IScoreCohesionApplicationPlugin {app};
}

QStringList iscore_plugin_cohesion::required() const
{
    return {"Scenario"};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_cohesion::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{IScoreCohesionCommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_cohesion_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
