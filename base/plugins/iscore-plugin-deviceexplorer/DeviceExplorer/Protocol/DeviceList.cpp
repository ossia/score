#include "DeviceList.hpp"
#include <algorithm>

template<typename TheList>
static auto get_device_iterator_by_name(
        const QString& name,
        const TheList& devlist)
{
    return std::find_if(devlist.cbegin(),
                        devlist.cend(),
                        [&] (DeviceInterface* d) { return d->settings().name == name; });
}

// TODO directly return iterator
bool DeviceList::hasDevice(const QString &name) const
{
    return get_device_iterator_by_name(name, m_devices) != m_devices.cend();
}

DeviceInterface &DeviceList::device(const QString &name) const
{
    auto it = get_device_iterator_by_name(name, m_devices);
    ISCORE_ASSERT(it != m_devices.cend());

    return **it;
}

void DeviceList::addDevice(DeviceInterface *dev)
{
    m_devices.push_back(dev);
}

void DeviceList::removeDevice(const QString &name)
{
    auto it = get_device_iterator_by_name(name, m_devices);
    ISCORE_ASSERT(it != m_devices.end());

    delete *it;
    m_devices.erase(it);
}

const std::vector<DeviceInterface *> &DeviceList::devices() const
{
    return m_devices;
}
