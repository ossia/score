#include "GUIItem.hpp"
#include <Models/WidgetAddressSetup.hpp>
#include <RemoteContext.hpp>
#include <WebSocketClient.hpp>
#include <State/Expression.hpp>
namespace RemoteUI
{

GUIItem::GUIItem(Context& ctx, WidgetKind c, QQuickItem* it):
  m_context{ctx},
  m_compType{c},
  m_item{it}
{

}

void GUIItem::setAddress(
    const Device::FullAddressSettings& addr)
{
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
}

void GUIItem::on_impulse()
{
  sendMessage(
        State::Message{
          State::AddressAccessor{m_addr.address},
          State::Value::fromValue(State::impulse_t{})});
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


void GUIItem::sendMessage(const State::Message& m)
{
  Serializer<JSONObject> s;
  s.readFrom(m);

  s.m_obj[iscore::StringConstant().Message] = iscore::StringConstant().Message;
  m_context.websocket.socket().sendTextMessage(QJsonDocument(s.m_obj).toJson());
}

}
