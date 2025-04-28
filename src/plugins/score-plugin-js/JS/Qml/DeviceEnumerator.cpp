#include "DeviceEnumerator.hpp"

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QtQml>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::GlobalDeviceEnumerator)
W_OBJECT_IMPL(JS::DeviceIdentifier)
W_OBJECT_IMPL(JS::DeviceListener)
namespace JS
{
/**
  var e = Score.enumerateDevices();
  e.enumerate = true;
  console.log(e.devices);
 */

GlobalDeviceEnumerator::GlobalDeviceEnumerator() { }

void GlobalDeviceEnumerator::setDeviceType(const QString& uid)
{
  // Convert the textual UUID in a strongly typed UUID
  try
  {
    auto uuid = uid.toUtf8();
    auto score_uid = score::uuids::string_generator::compute(uuid.begin(), uuid.end());
    if(score_uid.is_nil())
      return;
    this->m_deviceType = UuidKey<Device::ProtocolFactory>{score_uid};
  }
  catch(...)
  {
    qDebug("Error while parsing device type");
    return;
  }
  reprocess();
}

GlobalDeviceEnumerator::~GlobalDeviceEnumerator()
{
  m_enumerate = false;
  reprocess();
}

void GlobalDeviceEnumerator::setContext(const score::DocumentContext* doc)
{
  this->doc = doc;
  reprocess();
}

QQmlListProperty<DeviceIdentifier> GlobalDeviceEnumerator::devices()
{
  using CountFunction = QQmlListProperty<DeviceIdentifier>::CountFunction;
  using AtFunction = QQmlListProperty<DeviceIdentifier>::AtFunction;

  CountFunction count = +[](QQmlListProperty<DeviceIdentifier>* d) -> qsizetype {
    return ((GlobalDeviceEnumerator*)d->object)->m_raw_list.size();
  };
  AtFunction at = +[](QQmlListProperty<DeviceIdentifier>* d,
                      qsizetype index) -> DeviceIdentifier* {
    auto self = (GlobalDeviceEnumerator*)d->object;
    if(index >= 0 && index < std::ssize(self->m_raw_list))
      return self->m_raw_list[index];
    else
      return nullptr;
  };

  return QQmlListProperty<DeviceIdentifier>(this, nullptr, count, at);
}

void GlobalDeviceEnumerator::setEnumerate(bool b)
{
  if(m_enumerate == b)
    return;

  m_enumerate = b;
  enumerateChanged(b);

  QMetaObject::invokeMethod(this, &GlobalDeviceEnumerator::reprocess);
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
  for(auto d : m_raw_list)
    delete d;
  m_raw_list.clear();

  if(!this->doc)
  {
    doc = score::GUIAppContext().currentDocument();
    if(!doc)
      return;
  }
  if(!this->m_enumerate)
    return;

  auto& doc = *this->doc;
  for(auto& protocol : doc.app.interfaces<Device::ProtocolFactoryList>())
  {
    if(m_deviceType != Device::ProtocolFactory::ConcreteKey{})
      if(m_deviceType != protocol.concreteKey())
        continue;

    auto enums = protocol.getEnumerators(doc);
    for(auto& [category, enumerator] : enums)
    {
      auto on_deviceAdded
          = [this, category = category, proto = &protocol](
                const QString& name, const Device::DeviceSettings& devs) {
        this->deviceAdded(proto, category, name, devs);
        this->m_known_devices[proto].emplace_back(category, name, devs);
        m_raw_list.push_back(new DeviceIdentifier{category, name, devs, proto});
      };
      connect(
          enumerator, &Device::DeviceEnumerator::deviceAdded, this, on_deviceAdded,
          Qt::QueuedConnection);
      connect(
          enumerator, &Device::DeviceEnumerator::deviceRemoved, this,
          [this, category = category, proto = &protocol](const QString& name) {
        this->deviceRemoved(proto, name);
        {
          auto& vec = this->m_known_devices[proto];
          auto it = ossia::find_key(vec, category, name);
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

      enumerator->enumerate(on_deviceAdded);
    }
  }
}

DeviceListener::DeviceListener()
{
  auto doc = score::GUIAppContext().currentDocument();
  if(!doc)
    return;
  ctx = doc;
}

void DeviceListener::init()
{
  auto& plug = ctx->plugin<Explorer::DeviceDocumentPlugin>();
  auto& list = plug.list();
  for(auto& dev : list.devices())
  {
    on_deviceAdded(*dev);
  }

  connect(
      &list, &Device::DeviceList::deviceAdded, this,
      [this](Device::DeviceInterface* dev) { on_deviceAdded(*dev); });
  connect(
      &list, &Device::DeviceList::deviceRemoved, this,
      [](Device::DeviceInterface* dev) { });
}

QString addressFromParameter(const ossia::net::parameter_base& p)
{
  auto& dev = p.get_node().get_device();
  auto nm = QString::fromStdString(dev.get_name());
  auto str = QString::fromStdString(p.get_node().osc_address());
  return nm + ":" + str;
}

void DeviceListener::on_deviceAdded(Device::DeviceInterface& dev)
{
  auto ossia_dev = dev.getDevice();
  if(!ossia_dev)
  {
    return;
  }

  if(!this->m_name.isEmpty())
    if(ossia_dev->get_name() != this->m_name.toStdString())
      return;

  ossia_dev->on_node_created.connect<&DeviceListener::on_nodeCreated>(*this);
  ossia_dev->on_parameter_created.connect<&DeviceListener::on_parameterCreated>(*this);
}

void DeviceListener::on_nodeCreated(const ossia::net::node_base& n)
{
  if(auto param = n.get_parameter())
  {
    on_parameterCreated(*param);
  }
}

void DeviceListener::on_parameterCreated(const ossia::net::parameter_base& p)
{
  auto addr = addressFromParameter(p);
  parameterCreated(addr);
  const_cast<ossia::net::parameter_base&>(p).add_callback(
      [this, addr](const ossia::value& v) {
    auto vv = v.apply(ossia::qt::ossia_to_qvariant{});
    message(addr, vv);
  });
}

DeviceListener::~DeviceListener() { }

void DeviceListener::setDeviceName(const QString& b)
{
  if(b == m_name)
    return;
  m_name = b;
  deviceNameChanged(m_name);
}

void DeviceListener::setDeviceType(const QString& b)
{
  if(b == m_uuid)
    return;
  m_uuid = b;
  deviceTypeChanged(m_uuid);
}
void DeviceListener::setListen(bool b)
{
  if(b == m_listen)
    return;
  m_listen = b;
  listenChanged(m_listen);

  init();
}
}
