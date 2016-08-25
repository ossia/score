#include <Device/Protocol/DeviceInterface.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include "DeviceList.hpp"

namespace Device
{
template<typename TheList>
static auto get_device_iterator_by_name(
        const QString& name,
        const TheList& devlist)
{
    return find_if(devlist,
                   [&] (DeviceInterface* d) {
        return d->settings().name == name;
    });
}

DeviceInterface& DeviceList::device(const QString &name) const
{
    if(m_localDevice && name == m_localDevice->name())
        return *m_localDevice;

    auto it = get_device_iterator_by_name(name, m_devices);
    ISCORE_ASSERT(it != m_devices.cend());

    return **it;
}

DeviceInterface& DeviceList::device(const Node& node) const
{
    return device(Device::deviceName(node));
}

DeviceInterface* DeviceList::findDevice(const QString &name) const
{
    if(m_localDevice && name == m_localDevice->name())
        return m_localDevice;

    auto it = get_device_iterator_by_name(name, m_devices);
    return it != m_devices.cend() ? *it : nullptr;
}

void DeviceList::addDevice(DeviceInterface *dev)
{
    if(dev == m_localDevice)
    {
        // ...
    }
    else
    {
        m_devices.push_back(dev);
    }

    connect(dev, &DeviceInterface::logInbound,
            this, &DeviceList::logInbound);
    connect(dev, &DeviceInterface::logOutbound,
            this, &DeviceList::logOutbound);

    dev->setLogging(m_logging);
}

void DeviceList::removeDevice(const QString &name)
{
    if(m_localDevice && name == m_localDevice->name())
    {
        m_localDevice->setLogging(false);
        auto vec = m_localDevice->listening();
        for(const auto& elt : vec)
            m_localDevice->setListening(elt, false);
    }
    else
    {
        auto it = get_device_iterator_by_name(name, m_devices);
        ISCORE_ASSERT(it != m_devices.end());

        delete *it;
        m_devices.erase(it);
    }
}

const std::vector<DeviceInterface *> &DeviceList::devices() const
{
    return m_devices;
}

void DeviceList::setLogging(bool b)
{
    if(m_logging == b)
        return;
    m_logging = b;

    for(DeviceInterface* dev : m_devices)
        dev->setLogging(b);

    if(m_localDevice)
        m_localDevice->setLogging(b);
}

void DeviceList::setLocalDevice(DeviceInterface * dev)
{
    m_localDevice = dev;
}

void DeviceList::apply(std::function<void(Device::DeviceInterface&)> fun)
{
    for(DeviceInterface* dev : m_devices)
        fun(*dev);

    if(m_localDevice)
        fun(*m_localDevice);
}
}
