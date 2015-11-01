#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;
#include "DeviceExplorerControl.hpp"
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>


#include "Commands/Add/AddAddress.hpp"
#include "Commands/Add/AddDevice.hpp"
#include "Commands/Add/LoadDevice.hpp"
#include "Commands/Cut.hpp"
#include "Commands/Move.hpp"
#include "Commands/Paste.hpp"
#include "Commands/Remove/RemoveAddress.hpp"
#include "Commands/Remove.hpp"
#include "Commands/RemoveNodes.hpp"
#include "Commands/ReplaceDevice.hpp"
#include "Commands/UpdateAddresses.hpp"
#include "Commands/Update/UpdateAddressSettings.hpp"
#include "Commands/Update/UpdateDeviceSettings.hpp"


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
    std::pair<const std::string, CommandGeneratorMap> cmds;
    boost::mpl::for_each<
            boost::mpl::list<
            AddAddress,
            AddDevice,
            LoadDevice,
            UpdateAddressSettings,
            UpdateDeviceSettings,
            Cut,
//            Insert,
            Move,
            Paste,
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
