#pragma once

#include <interface/plugins/Autoconnect_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
#include <interface/plugins/PanelFactoryInterface_QtInterface.hpp>
#include <interface/plugins/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>
class NetworkSettings;

class NetworkPlugin :
    public QObject,
    public iscore::Autoconnect_QtInterface,
    public iscore::PluginControlInterface_QtInterface,
//		public iscore::PanelFactoryPluginInterface,
    public iscore::SettingsDelegateFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID Autoconnect_QtInterface_iid)
        Q_INTERFACES(iscore::Autoconnect_QtInterface
                     iscore::PluginControlInterface_QtInterface
//					 iscore::PanelFactoryPluginInterface
                     iscore::SettingsDelegateFactoryInterface_QtInterface)

    public:
        NetworkPlugin();
        virtual ~NetworkPlugin() = default;

        // Autoconnect interface
        virtual QList<iscore::Autoconnect> autoconnect_list() const override;

        // Settings interface
        virtual iscore::SettingsDelegateFactoryInterface* settings_make() override;

        // CustomCommand interface
        virtual QStringList control_list() const override;
        virtual iscore::PluginControlInterface* control_make(QString) override;

        /* Pour les groupes
        // Panel interface
        virtual QStringList panel_list() const override;
        virtual iscore::Panel* panel_make(QString name) override;
        */
};
