#pragma once

#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>
class NetworkSettings;

class NetworkPlugin :
    public QObject,
    public iscore::PluginControlInterface_QtInterface,
   // public iscore::SettingsDelegateFactoryInterface_QtInterface,
    public iscore::PanelFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PluginControlInterface_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PluginControlInterface_QtInterface
                     //iscore::SettingsDelegateFactoryInterface_QtInterface
                     iscore::PanelFactoryInterface_QtInterface)

    public:
        NetworkPlugin();

        //iscore::SettingsDelegateFactoryInterface* settings_make() override;

        iscore::PluginControlInterface* control() override;

        QList<iscore::PanelFactoryInterface*> panels() override;
};
