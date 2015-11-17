#include <NetworkPlugin.hpp>
#include <NetworkControl.hpp>
#include <settings_impl/NetworkSettings.hpp>
#include <core/application/Application.hpp>
#include "DistributedScenario/Panel/GroupPanelFactory.hpp"

#include "DistributedScenario/Group.hpp"
#define PROCESS_NAME "Network Process"

iscore_plugin_network::iscore_plugin_network() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {}//,
        //iscore::SettingsDelegateFactoryInterface_QtInterface {}
{
}

// Interfaces implementations :
//////////////////////////
/*
iscore::SettingsDelegateFactoryInterface* NetworkPlugin::settings_make()
{
    return new NetworkSettings;
}
*/
iscore::PluginControlInterface* iscore_plugin_network::make_control(iscore::Application& app)
{
    return new NetworkControl{app};
}

QList<iscore::PanelFactory*> iscore_plugin_network::panels()
{
    return {new GroupPanelFactory};
}



#include "DistributedScenario/Commands/AddClientToGroup.hpp"
#include "DistributedScenario/Commands/RemoveClientFromGroup.hpp"

#include "DistributedScenario/Commands/CreateGroup.hpp"
#include "DistributedScenario/Commands/RemoveGroup.hpp"

#include "DistributedScenario/Commands/ChangeGroup.hpp"
#include <iscore/command/CommandGeneratorMap.hpp>
std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_network::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{DistributedScenarioCommandFactoryName(), CommandGeneratorMap{}};
    boost::mpl::for_each<
            boost::mpl::list<
            AddClientToGroup,
            RemoveClientFromGroup,
            CreateGroup,
            ChangeGroup
            // TODO RemoveGroup;
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
