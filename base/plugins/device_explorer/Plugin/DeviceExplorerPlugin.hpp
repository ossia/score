#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <DeviceExplorer/Protocol/DeviceList.hpp>
#include "Singletons/SingletonProtocolList.hpp"

/*
class SingletonDeviceList
{
    public:
        static DeviceList& instance();
        static bool check(const QString& condition)
        {
            // TODO thorought testing required
            if(condition.isEmpty())
            {
                qDebug() << Q_FUNC_INFO << "Invalid condition: " << condition;
                return false;
            }

            QStringList spltCond = condition.split(" ");
            QStringList spltAddr = spltCond[0].split("/");

            // Get the corresponding device
            if(!instance().hasDevice(spltAddr[1]))
            {
                qDebug() << Q_FUNC_INFO << "Device '" << spltAddr[1] << "' not found";
                return false;
            }
            auto dev = instance().device(spltAddr[1]);

            spltAddr.removeFirst();
            spltAddr.removeFirst();

            spltCond.removeFirst();
            QString finalCond = spltAddr.join("/") + spltCond.join(" ");

            return dev->check(finalCond);
        }

        static void sendMessage(const QString& address, QVariant val)
        {
            // TODO thorought testing required
            if(address.isEmpty())
            {
                qDebug() << Q_FUNC_INFO << "Invalid address: " << address;
                return;
            }

            // Get the first part of the address
            QStringList splt = address.split("/");

            // Get the corresponding device
            if(!instance().hasDevice(splt[1]))
            {
                qDebug() << Q_FUNC_INFO << "Device '" << splt[1] << "' not found";
                return;
            }
            auto dev = instance().device(splt[1]);

            // Remove two first elements
            splt.removeFirst();
            splt.removeFirst();

            Message m;
            m.address = splt.join("/");
            m.value = val;
            dev->sendMessage(m);
        }

        static void sendMessage(const Message& m)
        {
            sendMessage(m.address, m.value);
        }

    private:
        static DeviceList m_instance;
};
*/
class DeviceExplorerPlugin :
    public QObject,
    public iscore::PanelFactory_QtInterface,
    public iscore::FactoryFamily_QtInterface,
    public iscore::FactoryInterface_QtInterface,
    public iscore::PluginControlInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactory_QtInterface_iid)
        Q_INTERFACES(
                iscore::PanelFactory_QtInterface
                iscore::FactoryFamily_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::PluginControlInterface_QtInterface)

    public:
        DeviceExplorerPlugin();

        // Panel interface
        QList<iscore::PanelFactory*> panels() override;

        // Factory for protocols
        QVector<iscore::FactoryFamily> factoryFamilies() override;

        // Contains the OSC, MIDI, Minuit factories
        QVector<iscore::FactoryInterface*> factories(const QString& factoryName) override;

        // Control
        iscore::PluginControlInterface* control() override;
};
