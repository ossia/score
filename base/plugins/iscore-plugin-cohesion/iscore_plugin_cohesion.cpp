#include "iscore_plugin_cohesion.hpp"
#include "IScoreCohesionControl.hpp"

#include <iscore_plugin_cohesion_commands_files.hpp>


iscore_plugin_cohesion::iscore_plugin_cohesion() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_cohesion::make_control(
        iscore::Application& app)
{
    return new IScoreCohesionControl {app};
}

QStringList iscore_plugin_cohesion::required() const
{
    return {"Scenario"};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_cohesion::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{IScoreCohesionCommandFactoryName(), CommandGeneratorMap{}};

    using Types = iscore::commands::TypeList<
  #include <iscore_plugin_cohesion_commands.hpp>
      >;
    iscore::commands::ForEach<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
