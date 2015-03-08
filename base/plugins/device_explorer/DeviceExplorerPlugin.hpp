#pragma once
#include <QObject>
#include <plugin_interface/plugins/PanelFactoryInterface_QtInterface.hpp>

class DeviceExplorerPlugin :
    public QObject,
    public iscore::PanelFactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PanelFactoryInterface_QtInterface)

    public:
        DeviceExplorerPlugin();
        virtual ~DeviceExplorerPlugin() = default;

        // Panel interface
        virtual QStringList panel_list() const override;
        virtual iscore::PanelFactoryInterface* panel_make(QString name) override;

};
