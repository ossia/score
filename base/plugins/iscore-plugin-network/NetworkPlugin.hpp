#pragma once

#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>
class NetworkSettings;

class iscore_plugin_network :
        public QObject,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::CommandFactory_QtInterface,
        // public iscore::SettingsDelegateFactoryInterface_QtInterface,
        public iscore::PanelFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID GUIApplicationContextPlugin_QtInterface_iid)
        Q_INTERFACES(
                iscore::GUIApplicationContextPlugin_QtInterface
                iscore::CommandFactory_QtInterface
                //iscore::SettingsDelegateFactoryInterface_QtInterface
                iscore::PanelFactory_QtInterface)

    public:
        iscore_plugin_network();

        //iscore::SettingsDelegateFactoryInterface* settings_make() override;

        iscore::GUIApplicationContextPlugin* make_applicationPlugin(iscore::Application& app) override;

        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;


        std::vector<iscore::PanelFactory*> panels() override;
};
