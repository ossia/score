#pragma once
#include <QObject>
#include <interface/plugins/PanelFactoryInterface_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
#include <interface/plugins/Autoconnect_QtInterface.hpp>

class DeviceExplorerPlugin :
    public QObject,
    public iscore::Autoconnect_QtInterface,
//		public iscore::PluginControlInterface_QtInterface,
    public iscore::PanelFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA (IID Autoconnect_QtInterface_iid)
        Q_INTERFACES (iscore::Autoconnect_QtInterface
//					 iscore::PluginControlInterface_QtInterface
                      iscore::PanelFactoryInterface_QtInterface)

    public:
        DeviceExplorerPlugin();
        virtual ~DeviceExplorerPlugin() = default;

        // Autoconnect interface
        virtual QList<iscore::Autoconnect> autoconnect_list() const override;

        // Plugin control interface
//		virtual QStringList control_list() const override;
//		virtual iscore::PluginControlInterface* control_make(QString) override;

        // Panel interface
        virtual QStringList panel_list() const override;
        virtual iscore::PanelFactoryInterface* panel_make (QString name) override;

};
