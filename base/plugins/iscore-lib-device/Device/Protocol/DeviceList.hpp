#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <QString>
#include <algorithm>
#include <vector>

#include <Device/Protocol/DeviceSettings.hpp>

namespace Device
{
class ISCORE_LIB_DEVICE_EXPORT DeviceList : public QObject
{
        Q_OBJECT
    public:
        auto find(const QString &name) const
        {
            return std::find_if(m_devices.cbegin(),
                                m_devices.cend(),
                                [&] (DeviceInterface* d) { return d->settings().name == name; });
        }


        DeviceInterface& device(const QString& name) const;
        DeviceInterface& device(const Device::Node& name) const;

        void addDevice(DeviceInterface* dev);
        void removeDevice(const QString& name);

        const std::vector<DeviceInterface*>& devices() const;

        void setLogging(bool);

    signals:
        void logInbound(const QString&) const;
        void logOutbound(const QString&) const;

    private:
        std::vector<DeviceInterface*> m_devices;
        bool m_logging = false;
};
}
