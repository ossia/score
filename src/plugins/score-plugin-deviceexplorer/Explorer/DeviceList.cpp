// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceList.hpp"

#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/Settings/ExplorerModel.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/algorithms.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Device::DeviceList)
namespace Device
{
template <typename TheList>
static auto get_device_iterator_by_name(const QString& name, const TheList& devlist)
{
  return ossia::find_if(devlist, [&](DeviceInterface* d) { return d->settings().name == name; });
}

DeviceInterface& DeviceList::device(const QString& name) const
{
  if (m_localDevice && name == m_localDevice->name())
    return *m_localDevice;
  if (m_audioDevice && name == m_audioDevice->name())
    return *m_audioDevice;

  auto it = get_device_iterator_by_name(name, m_devices);
  SCORE_ASSERT(it != m_devices.cend());

  return **it;
}

DeviceInterface& DeviceList::device(const Node& node) const
{
  return device(Device::deviceName(node));
}

DeviceInterface* DeviceList::findDevice(const QString& name) const
{
  if (m_localDevice && name == m_localDevice->name())
    return m_localDevice;
  if (m_audioDevice && name == m_audioDevice->name())
    return m_audioDevice;

  auto it = get_device_iterator_by_name(name, m_devices);
  return it != m_devices.cend() ? *it : nullptr;
}

void DeviceList::addDevice(DeviceInterface* dev)
{
  if (!dev)
    return;

  if (dev == m_localDevice || dev == m_audioDevice)
  {
    // ...
  }
  else if (
      dev->settings().protocol
      == UuidKey<Device::ProtocolFactory>{
          score::uuids::string_generator::compute("2835e6da-9b55-4029-9802-e1c817acbdc1")})
  {
    // FIXME
    // TODO dirty hack
    m_audioDevice = dev;
  }
  else
  {
    m_devices.push_back(dev);
  }

  connect(dev, &DeviceInterface::logInbound, this, &DeviceList::logInbound);
  connect(dev, &DeviceInterface::logOutbound, this, &DeviceList::logOutbound);

  dev->setLogging(get_cur_logging(m_logging));
  deviceAdded(*dev);
}

void DeviceList::removeDevice(const QString& name)
{
  if (m_localDevice && name == m_localDevice->name())
  {
    m_localDevice->setLogging(get_cur_logging(false));
    auto vec = m_localDevice->listening();
    for (const auto& elt : vec)
      m_localDevice->setListening(elt, false);
  }
  else if (m_audioDevice && name == m_audioDevice->name())
  {
    m_audioDevice->setLogging(get_cur_logging(false));
    auto vec = m_audioDevice->listening();
    for (const auto& elt : vec)
      m_audioDevice->setListening(elt, false);
  }
  else
  {
    auto it = get_device_iterator_by_name(name, m_devices);
    SCORE_ASSERT(it != m_devices.end());

    deviceRemoved(**it);
    delete *it;
    m_devices.erase(it);
  }
}

const std::vector<DeviceInterface*>& DeviceList::devices() const
{
  return m_devices;
}

void DeviceList::setLogging(bool b)
{
  if (m_logging == b)
    return;
  m_logging = b;

  for (DeviceInterface* dev : m_devices)
    dev->setLogging(get_cur_logging(b));

  if (m_localDevice)
    m_localDevice->setLogging(get_cur_logging(b));
  if (m_audioDevice)
    m_audioDevice->setLogging(get_cur_logging(b));
}

void DeviceList::setLocalDevice(DeviceInterface* dev)
{
  m_localDevice = dev;
}

void DeviceList::apply(std::function<void(Device::DeviceInterface&)> fun)
{
  for (DeviceInterface* dev : m_devices)
    fun(*dev);

  if (m_localDevice)
    fun(*m_localDevice);
  if (m_audioDevice)
    fun(*m_audioDevice);
}

DeviceLogging get_cur_logging(bool b)
{
  if (b)
  {
    static const Explorer::Settings::DeviceLogLevel ll;
    auto& app = score::GUIAppContext().settings<Explorer::Settings::Model>();
    auto log = app.getLogLevel();
    if (log == ll.logNothing)
      return LogNothing;
    if (log == ll.logEverything)
      return LogEverything;
    if (log == ll.logUnfolded)
      return LogUnfolded;
  }
  return LogNothing;
}
}
