#pragma once
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

#include <QPointF>

#include <cmath>

#include <cstdint>
// FIXME put the entirety of this as dynamic behaviour instead
namespace Process
{

static const constexpr int MaxRowsInEffect = 5;

// TODO not very efficient since it recomputes everything every time...
// Does a grid layout with maximum N rows per column.
template <typename F>
QPointF currentWidgetPos(int controlIndex, F getControlSize) noexcept(
    noexcept(getControlSize(0)))
{
  int N = MaxRowsInEffect * (controlIndex / MaxRowsInEffect);
  qreal x = 0;
  for(int i = 0; i < N;)
  {
    qreal w = 0;
    for(int j = i; j < i + MaxRowsInEffect && j < N; j++)
    {
      auto sz = getControlSize(j);
      w = std::max(w, sz.width());
    }
    x += w;
    i += MaxRowsInEffect;
  }

  qreal y = 0;
  for(int j = N; j < controlIndex; j++)
  {
    auto sz = getControlSize(j);
    y += sz.height();
  }

  return {x, y};
}

template <
    typename CreatePort, typename CreateControl, typename GetControlSize,
    typename GetName, typename GetFactory>
struct ControlSetup
{
  CreatePort createPort;
  CreateControl createControl;
  GetControlSize getControlSize;
  GetName name;
  GetFactory getFactory;
};

template <typename... Args>
auto controlSetup(Args&&... args)
{
  if constexpr(sizeof...(Args) == 4)
  {
    return controlSetup(
        std::forward<Args>(args)...,
        [](const Process::PortFactoryList& portFactory, Process::Port& port)
            -> Process::PortFactory& { return *portFactory.get(port.concreteKey()); });
  }
  else
  {
    return ControlSetup<Args...>{std::forward<Args>(args)...};
  }
}

template <typename T>
[[deprecated]] auto createControl(
    int i,
    const auto& setup, // See ControlSetup
    T& port, const Process::PortFactoryList& portFactory, const Process::Context& doc,
    QGraphicsItem* parentItem, QObject* parent)
{
  // TODO put the port at the correct order wrt its index ?
  auto item = new score::EmptyRectItem{parentItem};

  // Create the port
  auto& fact = setup.getFactory(portFactory, port);
  auto portItem = setup.createPort(fact, port, doc, item, parent);

  // Create the label
  auto lab = Dataflow::makePortLabel(port, item);

  // Create the control
  struct Controls
  {
    score::EmptyRectItem* item{};
    Dataflow::PortItem* port{};
    QGraphicsItem* widg{};
    score::SimpleTextItem* label{};
    QRectF itemRect;
  };

  if(QGraphicsItem* widg = setup.createControl(fact, port, doc, item, parent))
  {
    widg->setParentItem(item);

    // Layout the items
    const qreal labelHeight = 10;
    const qreal labelWidth = lab->boundingRect().width();
    const auto wrect = widg->boundingRect();
    const qreal widgetHeight = wrect.height();
    const qreal widgetWidth = wrect.width();

    auto h = std::max(20., (qreal)(widgetHeight + labelHeight + 7.));
    auto w = std::max(90., std::max(25. + labelWidth, widgetWidth));

    portItem->setPos(8., 4.);
    lab->setPos(20., 2);
    widg->setPos(18., labelHeight + 5.);

    const auto itemRect = QRectF{0., 0, w, h};

    QPointF pos = Process::currentWidgetPos(i, setup.getControlSize);
    item->setPos(pos);
    item->setRect(itemRect);
    item->setToolTip(QString("%1\n%2").arg(port.name(), port.description()));

    return Controls{item, portItem, widg, lab, itemRect};
  }
  else
  {
    QRectF itemRect{0., 0, 90., 30.};
    QPointF pos = Process::currentWidgetPos(i, setup.getControlSize);
    portItem->setPos(8., 4.);
    lab->setPos(20., 2);
    item->setPos(pos);
    item->setRect(itemRect);
    item->setToolTip(QString("%1\n%2").arg(port.name(), port.description()));

    return Controls{item, portItem, nullptr, lab, itemRect};
  }
}

template <typename C, typename T>
[[deprecated]] static auto makeControl(
    C& ctrl, T& inlet, QGraphicsItem& parent, QObject& context,
    const Process::Context& doc, const Process::PortFactoryList& portFactory)
{
  auto item = new score::EmptyItem{&parent};

  // Port
  Process::PortFactory* fact = portFactory.get(inlet.concreteKey());
  auto port = fact->makePortItem(inlet, doc, item, &context);
  port->setPos(0, 1.);

  // Text
  auto lab = Dataflow::makePortLabel(inlet, item);
  lab->setPos(12, 0);

  // Control
  auto widg = ctrl.make_item(ctrl, inlet, doc, nullptr, &context);
  widg->setParentItem(item);
  widg->setPos(0, 12.);

  // Create a single control
  struct ControlItem
  {
    QGraphicsItem& root;
    Dataflow::PortItem& port;
    score::SimpleTextItem& text;
    decltype(*widg)& control;
  };
  return ControlItem{*item, *port, *lab, *widg};
}

template <typename C, typename T>
[[deprecated]] static auto makeControlNoText(
    C& ctrl, T& inlet, QGraphicsItem& parent, QObject& context,
    const Process::Context& doc, const Process::PortFactoryList& portFactory)
{
  auto item = new score::EmptyItem{&parent};

  // Control
  auto widg = ctrl.make_item(ctrl, inlet, doc, nullptr, &context);
  widg->setParentItem(item);

  // Port
  Process::PortFactory* fact = portFactory.get(inlet.concreteKey());
  auto port = fact->makePortItem(inlet, doc, item, &context);

  // Create a single control
  struct ControlItem
  {
    score::EmptyItem& root;
    Dataflow::PortItem& port;
    decltype(*widg)& control;
  };
  return ControlItem{*item, *port, *widg};
}
}
