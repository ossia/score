
#include <State/MessageListSerialization.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <ControlSurface/CommandFactory.hpp>
#include <ControlSurface/Commands.hpp>
#include <ControlSurface/Presenter.hpp>
#include <ControlSurface/Process.hpp>
#include <ControlSurface/View.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/ssize.hpp>

#include <QTimer>
#pragma once
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

#include <QPointF>

#include <cmath>

#include <cstdint>
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

namespace ControlSurface
{
struct Presenter::Port
{
  score::EmptyRectItem* root;
  Dataflow::PortItem* port;
  QRectF rect;
};

Presenter::Presenter(
    const Model& layer, View* view, const Process::Context& ctx, QObject* parent)
    : Process::LayerPresenter{layer, view, ctx, parent}
    , m_view{view}
{
  connect(view, &View::addressesDropped, this, [this, &layer](const QMimeData* msg) {
    Mime<State::MessageList>::Deserializer des(*msg);
    auto lst = des.deserialize();
    if(lst.empty())
      return;

    auto& docctx = this->context().context;
    MacroCommandDispatcher<AddControlMacro> disp{docctx.commandStack};
    auto ids = getStrongIdRangePtr<Process::Port>(lst.size(), m_process.inlets());
    for(std::size_t i = 0; i < lst.size(); i++)
    {
      disp.submit(new AddControl{docctx, std::move(ids[i]), layer, std::move(lst[i])});
    }
    disp.commit();
  });

  connect(
      &layer, &Process::ProcessModel::controlAdded, this, &Presenter::on_controlAdded);

  connect(
      &layer, &Process::ProcessModel::controlRemoved, this,
      &Presenter::on_controlRemoved);

  auto& portFactory = ctx.app.interfaces<Process::PortFactoryList>();
  for(auto& e : layer.inlets())
  {
    auto inlet = qobject_cast<Process::ControlInlet*>(e);
    if(!inlet)
      continue;

    setupInlet(*inlet, portFactory, ctx);
  }
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setOpacity(1);
}

void Presenter::putBehind()
{
  m_view->setOpacity(0.2);
}

void Presenter::on_zoomRatioChanged(ZoomRatio) { }

void Presenter::parentGeometryChanged() { }

void Presenter::setupInlet(
    Process::ControlInlet& port, const Process::PortFactoryList& portFactory,
    const Process::Context& doc)
{
  // Main item creation
  int i = std::ssize(m_ports);

  auto csetup = Process::controlSetup(
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
    return factory.makePortItem(inlet, doc, item, parent);
      },
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
    return factory.makeControlItem(inlet, doc, item, parent);
  },
       [&](int j) { return m_ports[j].rect.size(); }, [&] { return port.name(); });
  auto [item, portItem, widg, lab, itemRect]
      = Process::createControl(i, csetup, port, portFactory, doc, m_view, this);

  // Remove button
  {
    const auto& pixmaps = Process::Pixmaps::instance();
    auto rm_item
        = new score::QGraphicsPixmapButton{pixmaps.close_on, pixmaps.close_off, item};
    connect(rm_item, &score::QGraphicsPixmapButton::clicked, &port, [this, &port] {
      QTimer::singleShot(0, &port, [this, &port] {
        CommandDispatcher<> disp{m_context.context.commandStack};
        disp.submit<RemoveControl>(static_cast<const Model&>(m_process), port);
      });
    });

    rm_item->setPos(8., 16.);
  }

  m_ports.push_back(Port{item, portItem, itemRect});
  // TODO updateRect();

  // TODO con(inlet, &Process::ControlInlet::domainChanged, this, [this,
  // &inlet] {
  // TODO   on_controlRemoved(inlet);
  // TODO   on_controlAdded(inlet.id());
  // TODO });
}

void Presenter::on_controlAdded(const Id<Process::Port>& id)
{
  auto& portFactory = m_context.context.app.interfaces<Process::PortFactoryList>();
  auto inlet = safe_cast<Process::ControlInlet*>(m_process.inlet(id));
  setupInlet(*inlet, portFactory, m_context.context);
}

void Presenter::on_controlRemoved(const Process::Port& port)
{
  for(auto it = m_ports.begin(); it != m_ports.end(); ++it)
  {
    auto ptr = it->port;
    if(&ptr->port() == &port)
    {
      auto parent_item = it->root;
      auto h = parent_item->boundingRect().height();
      delete parent_item;
      it = m_ports.erase(it);

      for(; it != m_ports.end(); ++it)
      {
        auto item = it->root;
        item->moveBy(0., -h);
      }
      return;
    }
  }
}

}
