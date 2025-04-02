#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <JS/Qml/DeviceEnumerator.hpp>
#include <JS/Qml/EditContext.hpp>
#include <Protocols/OSC/OSCProtocolFactory.hpp>

#include <ossia-config.hpp>
#if defined(OSSIA_PROTOCOL_WEBSOCKETS)
#include <Protocols/WS/WSProtocolFactory.hpp>
#include <Protocols/WS/WSSpecificSettings.hpp>
#endif

#if defined(OSSIA_PROTOCOL_SERIAL)
#include <Protocols/Serial/SerialProtocolFactory.hpp>
#include <Protocols/Serial/SerialSpecificSettings.hpp>
#endif

// #include <Protocols/OSC/OSCProtocolFactory.hpp>
// #include <Protocols/OSC/OSCSpecificSettings.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/Commands/RemoveNodes.hpp>

#include <Protocols/OSC/OSCSpecificSettings.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/parameter_data.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/oscquery/detail/json_writer.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/preset/preset.hpp>

#include <QJsonDocument>
namespace JS
{
GlobalDeviceEnumerator* EditJsContext::enumerateDevices()
{
  auto doc = ctx();
  if(!doc)
    return nullptr;

  auto e = new GlobalDeviceEnumerator{};
  e->setContext(doc);
  // e->setEnumerate(true);
  return e;
}
GlobalDeviceEnumerator* EditJsContext::enumerateDevices(const QString& uuid)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;

  auto e = new GlobalDeviceEnumerator{uuid};
  e->setContext(doc);
  // e->setEnumerate(true);
  return e;
}

void EditJsContext::removeDevice(QString name)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();

  auto cmd = new Explorer::Command::RemoveNodes;
  for(auto& node : plug.explorer().rootNode().children())
  {
    if(node.is<Device::DeviceSettings>())
    {
      cmd->addCommand(new Explorer::Command::Remove{plug, node});
    }
  }

  auto [m, _] = macro(*doc);
  submit(*m, cmd);
}

void EditJsContext::createOSCDevice(QString name, QString ip, int i, int o)
{
  auto doc = ctx();
  if(!doc)
    return;

  auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.protocol = Protocols::OSCProtocolFactory::static_concreteKey();

  Protocols::OSCSpecificSettings osc;
  ossia::net::udp_configuration udp;
  udp.local
      = ossia::net::inbound_socket_configuration{.bind = "0.0.0.0", .port = (uint16_t)i};
  udp.remote = ossia::net::outbound_socket_configuration{ip.toStdString(), (uint16_t)o};
  osc.configuration.transport = udp;
  set.deviceSpecificSettings = QVariant::fromValue(osc);

  auto [m, _] = macro(*doc);
  submit(*m, new Explorer::Command::LoadDevice{plug, std::move(set)});
}

void EditJsContext::createDevice(QString name, QString uuid, QJSValue obj)
{
  auto doc = ctx();
  if (!doc)
    return;

  auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.protocol = UuidKey<Device::ProtocolFactory>::fromString(uuid);
  auto json = QJsonDocument::fromVariant(obj.toVariant().value<QVariantMap>()).toJson();

  auto& pl = score::AppContext().interfaces<Device::ProtocolFactoryList>();

  if(auto prot = pl.get(set.protocol))
  {
    auto json_doc = readJson(json);
    JSONWriter wrt{json_doc};
    set.deviceSpecificSettings = prot->makeProtocolSpecificSettings(wrt.toVariant());
    if(set.deviceSpecificSettings != QVariant{})
    {
      auto [m, _] = macro(*doc);
      submit(*m, new Explorer::Command::LoadDevice{plug, std::move(set)});
    }
  }
}

void EditJsContext::createQMLWebSocketDevice(QString name, QString text)
{
#if defined(OSSIA_PROTOCOL_WEBSOCKETS)
  auto doc = ctx();
  if(!doc)
    return;

  auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.deviceSpecificSettings
      = QVariant::fromValue(Protocols::WSSpecificSettings{"", text});
  set.protocol = Protocols::WSProtocolFactory::static_concreteKey();

  auto [m, _] = macro(*doc);
  submit(*m, new Explorer::Command::LoadDevice{plug, std::move(set)});
#endif
}

void EditJsContext::createQMLSerialDevice(QString name, QString port, QString text)
{
#if defined(OSSIA_PROTOCOL_SERIAL)
  auto doc = ctx();
  if(!doc)
    return;

  QSerialPortInfo info;
  for(auto& p : QSerialPortInfo::availablePorts())
  {
    if(p.portName() == port)
    {
      info = p;
      break;
    }
  }
  if(info.isNull())
  {
    qDebug() << "Serial port " << port << " was not found";
    return;
  }

  auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.deviceSpecificSettings
      = QVariant::fromValue(Protocols::SerialSpecificSettings{info, text, 0});
  set.protocol = Protocols::SerialProtocolFactory::static_concreteKey();

  auto [m, _] = macro(*doc);
  submit(*m, new Explorer::Command::LoadDevice{plug, std::move(set)});
#endif
}

QString EditJsContext::deviceToJson(QString name)
{
  auto doc = ctx();
  if(!doc)
    return {};

  Explorer::DeviceDocumentPlugin& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  if(auto dev = plug.list().findDevice(name))
  {
    if(auto d = dev->getDevice())
    {
      auto p = ossia::presets::make_json_preset(
          d->get_root_node(), ossia::presets::preset_save_options{true, true, true});
      return QString::fromStdString(p);
    }
  }
  return {};
}

QString EditJsContext::deviceToOSCQuery(QString name)
{
  auto doc = ctx();
  if(!doc)
    return {};

  Explorer::DeviceDocumentPlugin& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  if(auto dev = plug.list().findDevice(name))
  {
    if(auto d = dev->getDevice())
    {
      auto p = ossia::oscquery::json_writer::query_namespace(d->get_root_node());
      return QString::fromUtf8(p.GetString(), p.GetLength());
    }
  }
  return {};
}

void EditJsContext::createAddress(QString addr, QString type)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto a = State::Address::fromString(addr);
  if(!a)
    return;

  auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  auto [m, _] = macro(*doc);

  Device::FullAddressSettings set;
  set.address = *a;

  const ossia::net::parameter_data* t
      = ossia::default_parameter_for_type(type.toStdString());
  if(t)
  {
    set.unit = t->unit;
    if(t->bounding)
      set.clipMode = *t->bounding;
    if(t->domain)
      set.domain = *t->domain;

    set.ioType = ossia::access_mode::BI;
    set.value = t->value;
    if(set.value.get_type() == ossia::val_type::NONE)
    {
      set.value = ossia::init_value(ossia::underlying_type(t->type));
    }
  }
  submit(*m, new Explorer::Command::AddWholeAddress{plug, std::move(set)});
}
}
