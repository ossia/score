// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GUIItem.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Models/NodeModel.hpp>
#include <Models/WidgetAddressSetup.hpp>
#include <RemoteApplication.hpp>
#include <RemoteContext.hpp>
#include <State/Expression.hpp>
#include <WebSocketClient.hpp>
namespace RemoteUI
{

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
  m_ctx.application.disableListening(m_addr, this);

  m_item->deleteLater();
}

void GUIItem::setAddress(const Device::FullAddressSettings& addr)
{
  m_ctx.application.disableListening(m_addr, this);
  m_addr = addr;

  QObject::disconnect(m_connection);
  switch (m_compType)
  {
    case WidgetKind::HSlider:
    {
      addr.value.apply(SetSliderAddress{*this, addr});
      break;
    }
    case WidgetKind::VSlider:
    {
      addr.value.apply(SetSliderAddress{*this, addr});
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
      m_ctx.application.enableListening(m_addr, this);
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

void GUIItem::on_impulse()
{
  sendMessage(State::Message{State::AddressAccessor{m_addr.address},
                             ossia::value(State::impulse{})});
}

void GUIItem::on_boolValueChanged(bool b)
{
  sendMessage(
      State::Message{State::AddressAccessor{m_addr.address}, ossia::value(b)});
}

void GUIItem::on_floatValueChanged(qreal r)
{
  sendMessage(State::Message{State::AddressAccessor{m_addr.address},
                             ossia::value((float)r)});
}

void GUIItem::on_stringValueChanged(QString str)
{
  sendMessage(State::Message{State::AddressAccessor{m_addr.address},
                             ossia::value(str.toStdString())});
}

void GUIItem::on_parsableValueChanged(QString s)
{
  if (auto val = State::parseValue(s.toStdString()))
  {
    sendMessage(State::Message{State::AddressAccessor{m_addr.address}, *val});
  }
}

void GUIItem::on_intValueChanged(qreal r)
{
  sendMessage(State::Message{State::AddressAccessor{m_addr.address},
                             ossia::value((int)r)});
}

void GUIItem::setValue(const State::Message& m)
{
  if (m_compType == WidgetKind::Label)
    m.value.apply(SetLabelAddress{*this, m_addr});
}

void GUIItem::sendMessage(const State::Message& m)
{
  JSONObject::Serializer s;
  s.readFrom(m);

  s.obj[score::StringConstant().Message] = score::StringConstant().Message;
  m_ctx.websocket.socket().sendTextMessage(QJsonDocument(s.obj).toJson());
}
}
