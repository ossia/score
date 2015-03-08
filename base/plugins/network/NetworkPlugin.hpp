#pragma once

#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>
class NetworkSettings;

class NetworkPlugin :
    public QObject,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::SettingsDelegateFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PluginControlInterface_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PluginControlInterface_QtInterface
                     iscore::SettingsDelegateFactoryInterface_QtInterface)

    public:
        NetworkPlugin();
        virtual ~NetworkPlugin() = default;

        // Settings interface
        virtual iscore::SettingsDelegateFactoryInterface* settings_make() override;

        // CustomCommand interface
        virtual iscore::PluginControlInterface* control_make() override;


};
