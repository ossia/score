#include "iscore_plugin_deviceexplorer.hpp"
#include <Explorer/DeviceExplorerPanelFactory.hpp>
using namespace iscore;
#include "DeviceExplorerControl.hpp"
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <iscore_plugin_deviceexplorer_commands_files.hpp>

iscore_plugin_deviceexplorer::iscore_plugin_deviceexplorer() :
    QObject {},
iscore::PanelFactory_QtInterface {}
{
}

std::vector<PanelFactory*> iscore_plugin_deviceexplorer::panels()
{
    return {new DeviceExplorerPanelFactory};
}



std::vector<iscore::FactoryListInterface*> iscore_plugin_deviceexplorer::factoryFamilies()
{
    return {new DynamicProtocolList};

}

PluginControlInterface *iscore_plugin_deviceexplorer::make_control(
        iscore::Application& app)
{
    return new DeviceExplorerControl{app};
}


std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_deviceexplorer::make_commands()
{
    using namespace DeviceExplorer::Command;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{DeviceExplorerCommandFactoryName(), CommandGeneratorMap{}};

    using Types = iscore::commands::TypeList<
  #include <iscore_plugin_deviceexplorer_commands.hpp>
      >;
    iscore::commands::ForEach<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
