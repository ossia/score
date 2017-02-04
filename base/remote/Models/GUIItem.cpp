#include "GUIItem.hpp"
#include <Models/WidgetAddressSetup.hpp>
#include <RemoteContext.hpp>
#include <WebSocketClient.hpp>
#include <Models/NodeModel.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Expression.hpp>
#include <RemoteApplication.hpp>
namespace RemoteUI
{

GUIItem::GUIItem(Context& ctx, WidgetKind c, QQuickItem* it):
  m_ctx{ctx},
  m_compType{c},
  m_item{it}
{
  connect(m_item, SIGNAL(addressChanged(QString)),
          this, SLOT(setAddress(QString)));

  auto obj = m_item->findChild<QObject*>("addressLabel");
  if(obj)
  {
    connect(obj, SIGNAL(removeMe()),
            this, SIGNAL(removeMe()));
  }
}

GUIItem::~GUIItem()
{
  QObject::disconnect(m_connection);
  m_ctx.application.disableListening(m_addr, this);

  m_item->deleteLater();
}

void GUIItem::setAddress(
    const Device::FullAddressSettings& addr)
{
  m_ctx.application.disableListening(m_addr, this);
  m_addr = addr;

  QObject::disconnect(m_connection);
  switch(m_compType)
  {
    case WidgetKind::HSlider:
    {
      eggs::variants::apply(SetSliderAddress{*this, addr}, addr.value.val.impl());
      break;
    }
    case WidgetKind::VSlider:
    {
      eggs::variants::apply(SetSliderAddress{*this, addr}, addr.value.val.impl());
      break;
    }
    case WidgetKind::CheckBox:
    {
      eggs::variants::apply(SetCheckboxAddress{*this, addr}, addr.value.val.impl());
      break;
    }
    case WidgetKind::LineEdit:
    {
      eggs::variants::apply(SetLineEditAddress{*this, addr}, addr.value.val.impl());
      break;
    }
    case WidgetKind::Label:
    {
      eggs::variants::apply(SetLabelAddress{*this, addr}, addr.value.val.impl());
      m_ctx.application.enableListening(m_addr, this);
      // Not editable
      break;
    }
    case WidgetKind::PushButton:
    {
      m_connection = QObject::connect(
                       item(), SIGNAL(clicked()),
                       this, SLOT(on_impulse()));
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
  if(auto address = State::Address::fromString(data))
  {
    auto n = Device::try_getNodeFromAddress(m_ctx.nodes.rootNode(), *address);
    if(n)
    {
      auto as = n->target<Device::AddressSettings>();
      if(as && as->value.val.isValid())
      {
        setAddress(Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(*as, *address));
      }
    }
  }
}

void GUIItem::on_impulse()
{
  sendMessage(
        State::Message{
          State::AddressAccessor{m_addr.address},
          State::Value::fromValue(State::impulse{})});
}

void GUIItem::on_boolValueChanged(bool b)
{
  sendMessage(
        State::Message{
          State::AddressAccessor{m_addr.address},
          State::Value::fromValue(b)});
}

void GUIItem::on_floatValueChanged(qreal r)
{
  sendMessage(
        State::Message{
          State::AddressAccessor{m_addr.address},
          State::Value::fromValue((float) r)});
}

void GUIItem::on_stringValueChanged(QString str)
{
  sendMessage(
        State::Message{
          State::AddressAccessor{m_addr.address},
          State::Value::fromValue(str.toStdString())});
}

void GUIItem::on_parsableValueChanged(QString s)
{
  if(auto val = State::parseValue(s.toStdString()))
  {
    sendMessage(
          State::Message{
            State::AddressAccessor{m_addr.address},
            *val});
  }
}

void GUIItem::on_intValueChanged(qreal r)
{
  sendMessage(
        State::Message{
          State::AddressAccessor{m_addr.address},
          State::Value::fromValue((int) r)});
}

void GUIItem::setValue(const State::Message& m)
{
  if(m_compType == WidgetKind::Label)
      eggs::variants::apply(SetLabelAddress{*this, m_addr}, m.value.val.impl());
}


void GUIItem::sendMessage(const State::Message& m)
{
  JSONObject::Serializer s;
  s.readFrom(m);

  s.obj[iscore::StringConstant().Message] = iscore::StringConstant().Message;
  m_ctx.websocket.socket().sendTextMessage(QJsonDocument(s.obj).toJson());
}

}
