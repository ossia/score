#include "WidgetListModel.hpp"
#include "CentralItemModel.hpp"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QQmlProperty>
#include <Device/Node/DeviceNode.hpp>
#include <Models/NodeModel.hpp>
#include <ossia/network/domain/domain.hpp>
#include <WebSocketClient.hpp>
namespace RemoteUI
{
CentralItemModel::CentralItemModel(
    Context& ctx,
    QObject *parent) :
  QObject(parent),
  m_ctx{ctx}
{
  connect(ctx.centralItem, SIGNAL(createObject(QString, qreal, qreal)),
          this, SLOT(on_itemCreated(QString, qreal, qreal)));
  connect(ctx.centralItem, SIGNAL(createAddress(QString, qreal, qreal)),
          this, SLOT(on_addressCreated(QString, qreal, qreal)));
}

void CentralItemModel::on_itemCreated(QString data, qreal x, qreal y)
{
  auto it = std::find_if(m_ctx.widgets.begin(), m_ctx.widgets.end(),
                         [&] (RemoteUI::WidgetListData* obj)
  {
            return obj->name() == data;
});

  if(it != m_ctx.widgets.end())
  {
    WidgetListData& widget = *(*it);
    QQmlComponent& comp = *widget.component();

    // Create the object
    auto obj = (QQuickItem*)comp.create(m_ctx.engine.rootContext());
    QQmlProperty(obj, "parent").write(
          QVariant::fromValue((QObject*)m_ctx.centralItem));

    // Put its center where the mouse is
    QQmlProperty(obj, "x").write(x - obj->width() / 2.);
    QQmlProperty(obj, "y").write(y - obj->height() / 2.);

    m_guiItems.push_back(
          new GUIItem{m_ctx, widget.widgetKind(), obj});
  }
}

struct AddressItemFactory
{
  WidgetKind operator()(State::impulse_t i)
  {
    return WidgetKind::PushButton;
  }

  WidgetKind operator()(bool b)
  {
    return WidgetKind::CheckBox;
  }

  WidgetKind operator()(int i)
  {
    return WidgetKind::HSlider;
  }

  WidgetKind operator()(float f)
  {
    return WidgetKind::HSlider;
  }

  WidgetKind operator()(const std::string& s)
  {
    return WidgetKind::LineEdit;
  }

  WidgetKind operator()(char c)
  {
    return WidgetKind::LineEdit;
  }

  template<std::size_t N>
  WidgetKind operator()(std::array<float, N> c)
  {
    return WidgetKind::Missing;
  }

  WidgetKind operator()(const State::tuple_t& c)
  {
    return WidgetKind::Missing;
  }
};

struct SetSliderAddress
{
  GUIItem& item;
  const Device::FullAddressSettings& address;

  void operator()(State::impulse_t c)
  {
    // Do nothing
    item.m_connection = QObject::connect(item.item(), SIGNAL(clicked()),
                     &item, SLOT(on_impulse()));
  }
  void operator()(bool c)
  {
    QQmlProperty(item.item(), "minimumValue").write(0.);
    QQmlProperty(item.item(), "maximumValue").write(1.);
    QQmlProperty(item.item(), "value").write((qreal)c);
    item.m_connection = QObject::connect(item.item(), SIGNAL(toggled()),
                     &item, SLOT(on_impulse()));
  }

  void operator()(int i)
  {
    auto min = ossia::convert<int>(ossia::net::get_min(address.domain.get()));
    auto max = ossia::convert<int>(ossia::net::get_max(address.domain.get()));

    QQmlProperty(item.item(), "from").write((qreal)min);
    QQmlProperty(item.item(), "to").write((qreal)max);
    QQmlProperty(item.item(), "setpSize").write(1);
    QQmlProperty(item.item(), "value").write((qreal)i);

    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(qreal)),
                     &item, SLOT(on_intValueChanged(qreal)));
  }

  void operator()(float f)
  {
    auto min = ossia::convert<float>(ossia::net::get_min(address.domain.get()));
    auto max = ossia::convert<float>(ossia::net::get_max(address.domain.get()));

    QQmlProperty(item.item(), "from").write((qreal)min);
    QQmlProperty(item.item(), "to").write((qreal)max);
    QQmlProperty(item.item(), "value").write((qreal)f);

    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(qreal)),
                     &item, SLOT(on_floatValueChanged(qreal)));
  }

  void operator()(char c)
  {
    QQmlProperty(item.item(), "min_chars").write(1);
    QQmlProperty(item.item(), "max_chars").write(1);
    QQmlProperty(item.item(), "value").write(c);

  }
  void operator()(const std::string& s)
  {
    QQmlProperty(item.item(), "value").write(QString::fromStdString(s));
  }

  template<std::size_t N>
  void operator()(std::array<float, N> c)
  {
    // TODO
  }

  void operator()(const State::tuple_t& c)
  {
    // TODO
  }
};

QQuickItem* CentralItemModel::create(WidgetKind c)
{
  auto comp = m_ctx.widgets[c]->component();
  auto obj = (QQuickItem*)comp->create(m_ctx.engine.rootContext());
  QQmlProperty(obj, "parent").write(
        QVariant::fromValue((QObject*)m_ctx.centralItem));
  return obj;
}

void CentralItemModel::on_addressCreated(QString data, qreal x, qreal y)
{
  auto address = State::Address::fromString(data);
  auto n = Device::try_getNodeFromAddress(m_ctx.nodes.rootNode(), address);
  if(n)
  {
    auto as = n->target<Device::AddressSettings>();
    if(as && as->value.val.isValid())
    {
      // We try to create a relevant component according to the type of the value.
      auto comp_type = eggs::variants::apply(AddressItemFactory{}, as->value.val.impl());

      if(auto obj = create(comp_type))
      {
        auto item = new GUIItem{m_ctx, comp_type, obj};

        qDebug() << address;
        item->setAddress(Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(*as, address));

        // Put its center where the mouse is
        QQmlProperty(obj, "x").write(x - obj->width() / 2.);
        QQmlProperty(obj, "y").write(y - obj->height() / 2.);

        m_guiItems.push_back(item);
      }
    }
  }
}

GUIItem::GUIItem(Context& ctx, WidgetKind c, QQuickItem* it):
  m_context{ctx},
  m_compType{c},
  m_item{it}
{

}

void GUIItem::setAddress(
    const Device::FullAddressSettings& addr)
{
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
      break;
    }

    case WidgetKind::LineEdit:
    {
      break;
    }
    case WidgetKind::Label:
    {
      break;
    }
    case WidgetKind::PushButton:
    {
      break;
    }
    default:
      break;
  }

  m_addr = addr;

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
