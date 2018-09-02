// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GUIItem.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Models/NodeModel.hpp>
#include <Models/WidgetAddressSetup.hpp>
#include <RemoteApplication.hpp>
#include <RemoteContext.hpp>
#include <State/Expression.hpp>
#include <State/ValueParser.hpp>
#include <WebSocketClient.hpp>
namespace RemoteUI
{

template<typename Fun>
void apply_to_address(const ossia::value& v, const ossia::domain& d, const ossia::unit_t& u, Fun&& f)
{
  if(v.valid())
  {
    ossia::apply_nonnull([&] (const auto& v) {
      if(d)
      {
        ossia::apply_nonnull([&] (const auto& dom) {
          if(u)
          {
            ossia::apply_nonnull([&] (const auto& ds) {
              ossia::apply_nonnull([&] (const auto& u) {
                f(v, dom, u);
              }, ds);
            }, u.v);
          }
          else
          {
            f(v, dom, unused_t{});
          }
        }, d.v);
      }
      else
      {
        if(u)
        {
          ossia::apply_nonnull([&] (const auto& ds) {
            ossia::apply_nonnull([&] (const auto& u) {
              f(v, unused_t{}, u);
            }, ds);
          }, u.v);
        }
        else
        {
          f(v, unused_t{}, unused_t{});
        }
      }
    }, v);
  }
}

GUIItem::GUIItem(Context& ctx, WidgetKind c, QQuickItem* it)
    : m_ctx{ctx}, m_compType{c}, m_item{it}
{
  connect(
      m_item, SIGNAL(addressChanged(QString)), this,
      SLOT(setAddress(QString)));

  auto obj = m_item->findChild<QObject*>("addressLabel");
  if (obj)
  {
    connect(obj, SIGNAL(removeMe()), this, SIGNAL(removeMe()));
  }
}

GUIItem::~GUIItem()
{
  QObject::disconnect(m_connection);

  m_item->deleteLater();
}

void GUIItem::setAddress(const Device::FullAddressSettings& addr)
{
  m_addr = addr;

  QObject::disconnect(m_connection);
  switch (m_compType)
  {
    case WidgetKind::VSlider:
    case WidgetKind::HSlider:
    {
      apply_to_address(addr.value, addr.domain.get(), addr.unit.get(), SetSliderAddress{*this, addr});
      break;
    }
    case WidgetKind::CheckBox:
    {
      addr.value.apply(SetCheckboxAddress{*this, addr});
      break;
    }
    case WidgetKind::LineEdit:
    {
      addr.value.apply(SetLineEditAddress{*this, addr});
      break;
    }
    case WidgetKind::Label:
    {
      addr.value.apply(SetLabelAddress{*this, addr});
      // Not editable
      break;
    }
    case WidgetKind::PushButton:
    {
      m_connection = QObject::connect(
          item(), SIGNAL(clicked()), this, SLOT(on_impulse()));
      break;
    }
    default:
      break;
  }

  QQmlProperty(m_item, "label.text").write(addr.address.toString());
}

qreal GUIItem::x() const
{
  return QQmlProperty(m_item, "x").read().toReal();
}

qreal GUIItem::y() const
{
  return QQmlProperty(m_item, "y").read().toReal();
}

void GUIItem::setAddress(QString data)
{
  if (auto address = State::Address::fromString(data))
  {
    auto n = Device::try_getNodeFromAddress(m_ctx.nodes.rootNode(), *address);
    if (n)
    {
      auto as = n->target<Device::AddressSettings>();
      if (as && as->value.valid())
      {
        setAddress(Device::FullAddressSettings::make<
                   Device::FullAddressSettings::as_child>(*as, *address));
      }
    }
  }
}

void GUIItem::setValue(const State::Message& m)
{
  if (m_compType == WidgetKind::Label)
    m.value.apply(SetLabelAddress{*this, m_addr});
}

void GUIItem::on_impulse()
{
  sendMessage(m_addr.address, ossia::impulse{});
}

void GUIItem::on_boolValueChanged(bool b)
{
  sendMessage(m_addr.address, b);
}

void GUIItem::on_floatValueChanged(qreal r)
{
  sendMessage(m_addr.address, (float)r);
}

void GUIItem::on_stringValueChanged(QString str)
{
  sendMessage(m_addr.address, str.toStdString());
}

void GUIItem::on_parsableValueChanged(QString s)
{
  if (auto val = State::parseValue(s.toStdString()))
  {
    sendMessage(m_addr.address, *val);
  }
}

void GUIItem::on_intValueChanged(qreal r)
{
  sendMessage(m_addr.address, (int) r);
}


void GUIItem::sendMessage(const State::Address& m, const ossia::value& v)
{
  auto dev_name = m.device.toStdString();
  auto dev_it = ossia::find_if(m_ctx.device, [&] (const auto& dev) { return dev->get_name() == dev_name; });
  if(dev_it != m_ctx.device.end())
  {
    auto& dev = **dev_it;

    if(auto n = ossia::net::find_node(dev, ("/" + m.path.join("/")).toStdString()))
    {
      if(auto p = n->get_parameter())
      {
        p->push_value(v);
      }
    }
  }
}
}
