#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QGraphicsSceneHoverEvent>
#include <QMenu>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMimeData>
#include <QPainter>
#include <QMouseEvent>

#include <tsl/hopscotch_map.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::PortItem)

namespace Dataflow
{
void onCreateCable(
    const score::DocumentContext& ctx,
    Dataflow::PortItem* p1,
    Dataflow::PortItem* p2);

PortItem* PortItem::clickedPort;
PortItem::PortItem(
    Process::Port& p,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent)
  : QGraphicsItem{parent}, m_context{ctx}, m_port{p}, m_diam{8.}
{
  this->setCursor(QCursor());
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
  this->setToolTip(p.customData());

  auto& plug = ctx.plugin<Process::DocumentPlugin>();
  plug.ports().insert({&p, this});

  Path<Process::Port> path = p;
  for (auto c : plug.cables())
  {
    if (c.first->source().unsafePath() == path.unsafePath())
    {
      c.second->setSource(this);
      cables.push_back(c.second);
    }
    else if (c.first->sink().unsafePath() == path.unsafePath())
    {
      c.second->setTarget(this);
      cables.push_back(c.second);
    }
  }

  QObject::connect(
      this,
      &Dataflow::PortItem::contextMenuRequested,
      this,
      [&](QPointF sp, QPoint p) {
        auto menu = new QMenu{};
        setupMenu(*menu, ctx);
        menu->exec(p);
        menu->deleteLater();
      });

  setVisible(CableItem::g_cables_enabled);
}

PortItem::~PortItem()
{
  for (auto cable : cables)
  {
    if (cable->source() == this)
      cable->setSource(nullptr);
    if (cable->target() == this)
      cable->setTarget(nullptr);
  }
  auto& ctx = m_context;
  auto& plug = ctx.plugin<Process::DocumentPlugin>();
  auto& p = plug.ports();
  auto it = p.find(&m_port);
  if (it != p.end())
    p.erase(it);
}

void PortItem::setupMenu(QMenu&, const score::DocumentContext& ctx) {}

void PortItem::setPortVisible(bool b)
{
  m_visible = b;
  resetPortVisible();
}
void PortItem::resetPortVisible()
{
  bool v = m_visible && CableItem::g_cables_enabled;
  if (v != isVisible())
    setVisible(v);
}

QRectF PortItem::boundingRect() const
{
  constexpr auto max_diam = 13.;
  return {-max_diam / 2., -max_diam / 2., max_diam, max_diam};
}

void PortItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  static constexpr qreal smallRadius = 4.;
  static constexpr qreal largeRadius = 6.;
  static constexpr QRectF smallEllipse{
      -smallRadius, -smallRadius, 2. * smallRadius, 2. * smallRadius};
  static const QPolygonF smallEllipsePath{[] {
    QPainterPath p;
    p.addEllipse(smallEllipse);
    return p.simplified().toFillPolygon();
  }()};
  static constexpr QRectF largeEllipse{
      -largeRadius, -largeRadius, 2. * largeRadius, 2. * largeRadius};
  static const QPolygonF largeEllipsePath{[] {
    QPainterPath p;
    p.addEllipse(largeEllipse);
    return p.simplified().toFillPolygon();
  }()};

  painter->setRenderHint(QPainter::Antialiasing, true);

  auto& style = Process::Style::instance();
  switch (m_port.type)
  {
    case Process::PortType::Audio:
      painter->setPen(style.AudioPortPen);
      painter->setBrush(style.AudioPortBrush);
      break;
    case Process::PortType::Message:
      painter->setPen(style.DataPortPen);
      painter->setBrush(style.DataPortBrush);
      break;
    case Process::PortType::Midi:
      painter->setPen(style.MidiPortPen);
      painter->setBrush(style.MidiPortBrush);
      break;
  }

  painter->drawPolygon(m_diam == 8. ? smallEllipsePath : largeEllipsePath);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (this->contains(event->pos()))
  {
    switch (event->button())
    {
      case Qt::RightButton:
        contextMenuRequested(event->scenePos(), event->screenPos());
        break;
      case Qt::LeftButton:
      {
        for(const auto& cable : cables) {
          cable->setZValue(10);
        }
      }
      default:
        break;
    }
  }
  event->accept();
}

QLineF portDragLineCoords{};
struct DragLine : QGraphicsLineItem
{
public:
  DragLine(QLineF f)
    : QGraphicsLineItem{f}
  {
    setPen(QPen(QBrush{qRgb(200, 200, 210)}, 2, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap,
                Qt::PenJoinStyle::RoundJoin));

    setAcceptHoverEvents(true);
  }
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    portDragLineCoords.setP2(event->scenePos());
    setLine(portDragLineCoords);
  }
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
  {
    portDragLineCoords.setP2(event->scenePos());
    setLine(portDragLineCoords);
  }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    portDragLineCoords.setP2(event->scenePos());
    setLine(portDragLineCoords);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    QGraphicsLineItem::paint(painter, option, widget);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
};

QGraphicsLineItem* portDragLine{};
struct DragMoveFilter : QObject
{
public:
  bool eventFilter(QObject* watched, QEvent* event) override
  {
    if (event->type() == QEvent::GraphicsSceneDragMove) {
        auto ev = static_cast<QGraphicsSceneDragDropEvent *>(event);
        portDragLineCoords.setP2(ev->scenePos());
        portDragLine->setLine(portDragLineCoords);
        return false;
    } else {
        return QObject::eventFilter(watched, event);
    }
  }
};
DragMoveFilter* drag_move_filter{};
void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if (QLineF(pos(), event->pos()).length() > QApplication::startDragDistance())
  {
    QDrag* d{new QDrag{this}};
    QMimeData* m = new QMimeData;
    portDragLineCoords = QLineF{scenePos(), event->scenePos()};
    portDragLine = new DragLine{portDragLineCoords};

    scene()->installEventFilter(drag_move_filter = new DragMoveFilter{});
    scene()->addItem(portDragLine);
    clickedPort = this;
    m->setData(score::mime::port(), {});
    d->setMimeData(m);
    d->exec();

    connect(d, &QDrag::destroyed, this, [this] {
      scene()->removeEventFilter(drag_move_filter);
      clickedPort = nullptr;
      delete portDragLine;
      delete drag_move_filter;
    });
  }
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (this->contains(event->pos()))
  {
    event->accept();
    switch (event->button())
    {
      case Qt::LeftButton:
        score::SelectionDispatcher{
            score::IDocument::documentContext(m_port).selectionStack}
            .setAndCommit({&m_port});
        break;
      default:
        break;
    }
  }
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  event->accept();
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 12.;
  update();
  event->accept();
}

void PortItem::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void PortItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

QVariant PortItem::itemChange(
    QGraphicsItem::GraphicsItemChange change,
    const QVariant& value)
{
  switch (change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      for (auto cbl : cables)
      {
        cbl->resize();
      }
      break;
    case QGraphicsItem::ItemVisibleHasChanged:
    case QGraphicsItem::ItemSceneHasChanged:
      for (auto cbl : cables)
      {
        cbl->check();
      }
      break;
    default:
      break;
  }

  return QGraphicsItem::itemChange(change, value);
}
}
