// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CentralItemModel.hpp"

#include "WidgetListModel.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Models/NodeModel.hpp>
#include <Models/WidgetAddressSetup.hpp>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <WebSocketClient.hpp>
namespace RemoteUI
{
// Type to widget
struct AddressItemFactory
{
  auto operator()()
  {
    throw std::runtime_error("no item");
    return WidgetKind::PushButton;
  }
  auto operator()(ossia::impulse i)
  {
    return WidgetKind::PushButton;
  }

  auto operator()(bool b)
  {
    return WidgetKind::CheckBox;
  }

  auto operator()(int i)
  {
    return WidgetKind::HSlider;
  }

  auto operator()(float f)
  {
    return WidgetKind::HSlider;
  }

  auto operator()(const std::string& s)
  {
    return WidgetKind::LineEdit;
  }

  auto operator()(char c)
  {
    return WidgetKind::LineEdit;
  }

  template <std::size_t N>
  auto operator()(std::array<float, N> c)
  {
    return WidgetKind::Label;
  }

  auto operator()(const std::vector<ossia::value>& c)
  {
    return WidgetKind::Label;
  }
};

CentralItemModel::CentralItemModel(Context& ctx, QObject* parent)
    : QObject(parent), m_ctx{ctx}
{
  connect(
      ctx.centralItem, SIGNAL(createObject(QString, qreal, qreal)), this,
      SLOT(on_itemCreated(QString, qreal, qreal)));
  connect(
      ctx.centralItem, SIGNAL(createAddress(QString, qreal, qreal)), this,
      SLOT(on_addressCreated(QString, qreal, qreal)));
}

void CentralItemModel::on_itemCreated(QString data, qreal x, qreal y)
{
  auto it = std::find_if(
      m_ctx.widgets.begin(), m_ctx.widgets.end(),
      [&](RemoteUI::WidgetListData* obj) { return obj->name() == data; });

  if (it != m_ctx.widgets.end())
  {
    WidgetListData& widget = *(*it);
    QQmlComponent& comp = *widget.component();

    // Create the object
    auto obj = (QQuickItem*)comp.create(m_ctx.engine.rootContext());
    if (obj)
    {
      QQmlProperty(obj, "parent")
          .write(QVariant::fromValue((QObject*)m_ctx.centralItem));

      // Put its center where the mouse is
      QQmlProperty(obj, "x").write(x - obj->width() / 2.);
      QQmlProperty(obj, "y").write(y - obj->height() / 2.);

      addItem(new GUIItem{m_ctx, widget.widgetKind(), obj});
    }
    else
    {
      qDebug() << "Error: object " << data << "could not be created";
    }
  }
}

QQuickItem* CentralItemModel::create(WidgetKind c)
{
  auto comp = m_ctx.widgets[c]->component();
  auto obj = (QQuickItem*)comp->create(m_ctx.engine.rootContext());
  QQmlProperty(obj, "parent")
      .write(QVariant::fromValue((QObject*)m_ctx.centralItem));
  return obj;
}

void CentralItemModel::on_addressCreated(QString data, qreal x, qreal y)
{
  if (auto address = State::Address::fromString(data))
  {
    auto n = Device::try_getNodeFromAddress(m_ctx.nodes.rootNode(), *address);
    if (n)
    {
      auto as = n->target<Device::AddressSettings>();
      if (as && as->value.valid())
      {
        // We try to create a relevant component according to the type of the
        // value.
        auto comp_type = as->value.apply(AddressItemFactory{});

        if (auto obj = create(comp_type))
        {
          auto item = new GUIItem{m_ctx, comp_type, obj};

          item->setAddress(
              Device::FullAddressSettings::make<
                  Device::FullAddressSettings::as_child>(*as, *address));

          // Put its center where the mouse is
          QQmlProperty(obj, "x").write(x - obj->width() / 2.);
          QQmlProperty(obj, "y").write(y - obj->height() / 2.);

          addItem(item);
        }
      }
    }
  }
}

void CentralItemModel::addItem(GUIItem* item)
{
  m_guiItems.push_back(item);

  connect(
      item, &GUIItem::removeMe, this,
      [=] {
        m_guiItems.removeOne(item);
        item->deleteLater();
      },
      Qt::QueuedConnection);
}

void CentralItemModel::removeItem(GUIItem* item)
{
}
}
