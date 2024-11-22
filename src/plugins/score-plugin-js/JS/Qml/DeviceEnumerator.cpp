#include "DeviceEnumerator.hpp"

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::GlobalDeviceEnumerator)
namespace JS
{
/**
  var e = Score.enumerateDevices();
  e.enumerate = true;
  console.log(e.devices);
 */

GlobalDeviceEnumerator::GlobalDeviceEnumerator() { }
GlobalDeviceEnumerator::~GlobalDeviceEnumerator()
{
  m_enumerate = false;
  reprocess();
}

void GlobalDeviceEnumerator::setContext(const score::DocumentContext* doc)
{
  this->doc = doc;
  reprocess();
  //list = &doc.plugin<Explorer::DeviceDocumentPlugin>();
}

QQmlListProperty<DeviceIdentifier> GlobalDeviceEnumerator::devices()
{

  return QQmlListProperty<DeviceIdentifier>(
      this, nullptr, [](QQmlListProperty<DeviceIdentifier>* d) -> long long {
    qDebug() << "sz: " << ((GlobalDeviceEnumerator*)d->object)->m_raw_list.size();
    return ((GlobalDeviceEnumerator*)d->object)->m_raw_list.size();
  }, [](QQmlListProperty<DeviceIdentifier>* d, qsizetype index) -> DeviceIdentifier* {
    auto self = (GlobalDeviceEnumerator*)d->object;
    qDebug() << "sz: " << self->m_raw_list.size() << " => " << index;
    if(index >= 0 && index < self->m_raw_list.size())
      return self->m_raw_list[index];
    else
      return nullptr;
  });
}
void GlobalDeviceEnumerator::setEnumerate(bool b)
{
  if(m_enumerate == b)
    return;

  m_enumerate = b;
  enumerateChanged(b);

  reprocess();
}

void GlobalDeviceEnumerator::reprocess()
{
  for(auto& [k, v] : this->m_current_enums)
  {
    for(auto& [ename, e] : v)
      delete e;
  }
  this->m_current_enums.clear();
  this->m_known_devices.clear();

  if(!this->doc)
    return;
  if(!this->m_enumerate)
    return;

  auto& doc = *this->doc;
  for(auto& protocol : doc.app.interfaces<Device::ProtocolFactoryList>())
  {
    auto enums = protocol.getEnumerators(doc);
    for(auto& [name, enumerator] : enums)
    {
      connect(
          enumerator, &Device::DeviceEnumerator::deviceAdded, this,
          [this,
           proto = &protocol](const QString& name, const Device::DeviceSettings& devs) {
        this->deviceAdded(proto, name, devs);
        this->m_known_devices[proto].emplace_back(name, devs);
        m_raw_list.push_back(
            new DeviceIdentifier{.name = name, .settings = devs, .protocol = proto});
      },
          Qt::QueuedConnection);
      connect(
          enumerator, &Device::DeviceEnumerator::deviceRemoved, this,
          [this, proto = &protocol](const QString& name) {
        this->deviceRemoved(proto, name);
        {
          auto& vec = this->m_known_devices[proto];
          auto it = ossia::find_key(vec, name);
          if(it != vec.end())
            vec.erase(it);
        }
        {
          auto it = ossia::find_if(this->m_raw_list, [&](auto& di) {
            return di->protocol == proto && di->name == name;
          });
          if(it != this->m_raw_list.end())
          {
            delete *it;
            this->m_raw_list.erase(it);
          }
        }
      }, Qt::QueuedConnection);
    }
  }
}
}
