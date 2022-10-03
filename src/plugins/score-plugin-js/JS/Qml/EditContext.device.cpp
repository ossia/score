#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <JS/Qml/EditContext.hpp>

// #include <Protocols/OSC/OSCProtocolFactory.hpp>
// #include <Protocols/OSC/OSCSpecificSettings.hpp>
#include <Explorer/Commands/Add/AddAddress.hpp>
// #include <Explorer/Commands/Add/LoadDevice.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/parameter_data.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/preset/preset.hpp>
namespace JS
{

void EditJsContext::createOSCDevice(QString name, QString host, int in, int out)
{ /*
  auto doc = ctx();
  if (!doc)
    return;
  auto& plug = doc->plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.deviceSpecificSettings
      = QVariant::fromValue(Protocols::OSCSpecificSettings{in, out, host});
  set.protocol = Protocols::OSCProtocolFactory::static_concreteKey();

  auto [m, _] = macro();
  m->submit(new Explorer::Command::LoadDevice{plug, std::move(set)});
  */
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
