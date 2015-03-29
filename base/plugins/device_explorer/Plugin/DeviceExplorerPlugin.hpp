#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <DeviceExplorer/Protocol/ProtocolList.hpp>


class SingletonProtocolList
{
    public:
        static ProtocolList& instance();

    private:
        static ProtocolList m_instance;
};


class DeviceExplorerPlugin :
    public QObject,
    public iscore::PanelFactoryInterface_QtInterface,
    public iscore::FactoryFamily_QtInterface,
    public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::PanelFactoryInterface_QtInterface
                iscore::FactoryFamily_QtInterface
                iscore::FactoryInterface_QtInterface)

    public:
        DeviceExplorerPlugin();
        virtual ~DeviceExplorerPlugin() = default;

        // Panel interface
        virtual QStringList panel_list() const override;
        virtual iscore::PanelFactoryInterface* panel_make(QString name) override;


        // Factory for protocols
        virtual QVector<iscore::FactoryFamily> factoryFamilies_make() override;

        // Contains the OSC, MIDI, Minuit factories
        virtual QVector<iscore::FactoryInterface*> factories_make(QString factoryName) override;

    private:
        SingletonProtocolList m_protocols;

};
