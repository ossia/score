#include <Explorer/DeviceExplorerPanelFactory.hpp>

#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore_plugin_deviceexplorer_commands_files.hpp>
#include "iscore_plugin_deviceexplorer.hpp"

#include <Device/Protocol/ProtocolList.hpp>
#include <unordered_map>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

#include <Explorer/Listening/ListeningHandlerFactoryList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include "DeviceExplorerApplicationPlugin.hpp"

namespace iscore {

class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

iscore_plugin_deviceexplorer::iscore_plugin_deviceexplorer() :
    QObject {},
iscore::PanelFactory_QtInterface {}
{
    QMetaType::registerComparators<Device::ProtocolFactoryKey>();
}

iscore_plugin_deviceexplorer::~iscore_plugin_deviceexplorer()
{

}

std::vector<iscore::PanelFactory*> iscore_plugin_deviceexplorer::panels()
{
    return {new Explorer::DeviceExplorerPanelFactory};
}



std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_deviceexplorer::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Device::DynamicProtocolList,
            Explorer::ListeningHandlerFactoryList>();

}


std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>>
iscore_plugin_deviceexplorer::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
        FW<iscore::DocumentPluginFactory,
             Explorer::DocumentPluginFactory>
    >>(ctx, key);
}

iscore::GUIApplicationContextPlugin *iscore_plugin_deviceexplorer::make_applicationPlugin(
        const iscore::ApplicationContext& app)
{
    return new Explorer::DeviceExplorerApplicationPlugin{app};
}


std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_deviceexplorer::make_commands()
{
    using namespace Explorer::Command;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{DeviceExplorerCommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
    #include <iscore_plugin_deviceexplorer_commands.hpp>
    >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}

int32_t iscore_plugin_deviceexplorer::version() const
{
    return 1;
}

UuidKey<iscore::Plugin> iscore_plugin_deviceexplorer::key() const
{
    return "3c2a0e25-ab14-4c06-a1ba-033d721a520f";
}
