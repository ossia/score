#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>

// TODO rename file
class iscore_plugin_deviceexplorer final :
        public QObject,
        public iscore::PanelFactory_QtInterface,
        public iscore::FactoryList_QtInterface,
        public iscore::PluginControlInterface_QtInterface,
        public iscore::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactory_QtInterface_iid)
        Q_INTERFACES(
                iscore::PanelFactory_QtInterface
                iscore::FactoryList_QtInterface
                iscore::PluginControlInterface_QtInterface
                iscore::CommandFactory_QtInterface)

    public:
        iscore_plugin_deviceexplorer();

        // Panel interface
        std::vector<iscore::PanelFactory*> panels() override;

        // Factory for protocols
        std::vector<iscore::FactoryListInterface*> factoryFamilies() override;

        // Control
        iscore::PluginControlInterface* make_control(iscore::Application& app) override;

        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;
};
