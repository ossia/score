#pragma once
#include <QList>
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>

class DeviceList
{
    public:

        void addDevice(DeviceInterface* dev)
        { m_devices.append(dev); }

        void removeDevice(const QString& name)
        { qDebug() << Q_FUNC_INFO << "TODO"; }

        QList<DeviceInterface*> devices() const
        { return m_devices; }
    private:
        QList<DeviceInterface*> m_devices;
};
