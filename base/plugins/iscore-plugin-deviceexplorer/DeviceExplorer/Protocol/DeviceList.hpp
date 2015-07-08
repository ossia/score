#pragma once
#include <QList>
#include <QDebug>
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>

class DeviceList
{
    public:
        DeviceList() = default;

        DeviceList(DeviceList&&) = delete;
        DeviceList(const DeviceList&) = delete;
        DeviceList& operator=(const DeviceList&) = delete;
        DeviceList& operator=(DeviceList&&) = delete;

        bool hasDevice(const QString& name) const;
        DeviceInterface& device(const QString& name) const;

        void addDevice(DeviceInterface* dev);
        void removeDevice(const QString& name);

        const std::vector<DeviceInterface*>& devices() const;

    private:
        std::vector<DeviceInterface*> m_devices;
};
