#pragma once
#include <QList>
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>

class DeviceList
{
    public:

        DeviceList() = default;

        DeviceList(DeviceList&&) = delete;
        DeviceList(const DeviceList&) = delete;
        DeviceList& operator=(const DeviceList&) = delete;
        DeviceList& operator=(DeviceList&&) = delete;

        bool hasDevice(const QString& name)
        {
            using namespace std;
            return find_if(begin(m_devices), end(m_devices),
                           [&] (DeviceInterface* d)
                           { return d->settings().name == name; })
                   != end(m_devices);
        }

        DeviceInterface* device(const QString& name)
        {
            using namespace std;
            return *find_if(begin(m_devices), end(m_devices),
                            [&] (DeviceInterface* d) { return d->settings().name == name; });
        }

        void addDevice(DeviceInterface* dev)
        { m_devices.append(dev); }

        void removeDevice(const QString& name)
        { qDebug() << Q_FUNC_INFO << "TODO"; }

        QList<DeviceInterface*> devices() const
        { return m_devices; }

    private:
        QList<DeviceInterface*> m_devices;
};
