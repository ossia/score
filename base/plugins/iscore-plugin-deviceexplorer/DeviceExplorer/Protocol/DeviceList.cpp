#include "DeviceList.hpp"
#include <boost/range/algorithm.hpp>

static auto get_device_iterator_by_name(
		const QString& name,
        QList<DeviceInterface*>& devlist)
{
	return boost::range::find_if(devlist, [&] (DeviceInterface* d) { return d->settings().name == name; });
}

bool DeviceList::hasDevice(const QString &name)
{
    return get_device_iterator_by_name(name, m_devices) != std::end(m_devices);
}

DeviceInterface &DeviceList::device(const QString &name)
{
	auto it = get_device_iterator_by_name(name, m_devices);
	Q_ASSERT(it != m_devices.end());

	return **it;
}

void DeviceList::addDevice(DeviceInterface *dev)
{
	m_devices.append(dev);
}

void DeviceList::removeDevice(const QString &name)
{
	auto it = get_device_iterator_by_name(name, m_devices);
	Q_ASSERT(it != m_devices.end());

	delete *it;
	m_devices.erase(it);
}

QList<DeviceInterface *> DeviceList::devices() const
{
	return m_devices;
}
