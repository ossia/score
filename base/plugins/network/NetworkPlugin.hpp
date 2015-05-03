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
    public iscore::PanelFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PluginControlInterface_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PluginControlInterface_QtInterface
                     //iscore::SettingsDelegateFactoryInterface_QtInterface
                     iscore::PanelFactory_QtInterface)

    public:
        NetworkPlugin();

        //iscore::SettingsDelegateFactoryInterface* settings_make() override;

        iscore::PluginControlInterface* control() override;

        QList<iscore::PanelFactory*> panels() override;
};
