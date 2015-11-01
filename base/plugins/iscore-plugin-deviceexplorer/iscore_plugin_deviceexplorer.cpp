#include "iscore_plugin_deviceexplorer.hpp"
#include <Explorer/DeviceExplorerPanelFactory.hpp>
using namespace iscore;
#include "DeviceExplorerControl.hpp"
#include <Device/Protocol/ProtocolFactoryInterface.hpp>


#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/AddDevice.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove/RemoveAddress.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/Commands/RemoveNodes.hpp>
#include <Explorer/Commands/ReplaceDevice.hpp>
#include <Explorer/Commands/UpdateAddresses.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>


iscore_plugin_deviceexplorer::iscore_plugin_deviceexplorer() :
    QObject {},
iscore::PanelFactory_QtInterface {}
{
}

QList<PanelFactory*> iscore_plugin_deviceexplorer::panels()
{
    return {new DeviceExplorerPanelFactory};
}



QVector<iscore::FactoryFamily> iscore_plugin_deviceexplorer::factoryFamilies()
{
    return {{ProtocolFactory::factoryName(),
            [] (iscore::FactoryInterface* f)
            { SingletonProtocolList::instance().registerFactory(f); }}};
}

PluginControlInterface *iscore_plugin_deviceexplorer::make_control(Presenter* pres)
{
    return new DeviceExplorerControl{pres};
}


std::pair<const std::string, CommandGeneratorMap> iscore_plugin_deviceexplorer::make_commands()
{
    using namespace DeviceExplorer::Command;
    std::pair<const std::string, CommandGeneratorMap> cmds{DeviceExplorerCommandFactoryName(), CommandGeneratorMap{}};
    boost::mpl::for_each<
            boost::mpl::list<
            AddAddress,
            AddDevice,
            LoadDevice,
            UpdateAddressSettings,
            UpdateDeviceSettings,
            Remove,
            RemoveAddress,
            RemoveNodes,
            ReplaceDevice,
            UpdateAddressesValues
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
