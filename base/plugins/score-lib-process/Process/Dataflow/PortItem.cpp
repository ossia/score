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
#include <QMimeData>
#include <QPainter>

#include <tsl/hopscotch_map.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::PortItem)

namespace Dataflow
{
void onCreateCable(
    const score::DocumentContext& ctx, Dataflow::PortItem* p1,
    Dataflow::PortItem* p2);

PortItem* PortItem::clickedPort;
PortItem::PortItem(
    Process::Port& p, const score::DocumentContext& ctx, QGraphicsItem* parent)
    : QGraphicsItem{parent}, m_context{ctx}, m_port{p}
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
      this, &Dataflow::PortItem::contextMenuRequested, this,
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

void PortItem::setupMenu(QMenu&, const score::DocumentContext& ctx)
{
}

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
  return {-m_diam / 2., -m_diam / 2., m_diam, m_diam};
}

void PortItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static const qreal smallRadius = 3.;
  static const qreal largeRadius = 4.;
  static const QRectF smallEllipse{-smallRadius, -smallRadius,
                                   2. * smallRadius, 2. * smallRadius};
  static const QPolygonF smallEllipsePath{[] {
    QPainterPath p;
    p.addEllipse(smallEllipse);
    return p.simplified().toFillPolygon();
  }()};
  static const QRectF largeEllipse{-largeRadius, -largeRadius,
                                   2. * largeRadius, 2. * largeRadius};
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

  painter->drawPolygon(m_diam == 6. ? smallEllipsePath : largeEllipsePath);
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
      default:
        break;
    }
  }
  event->accept();
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if (QLineF(pos(), event->pos()).length() > QApplication::startDragDistance())
  {
    QDrag* d{new QDrag{this}};
    QMimeData* m = new QMimeData;
    clickedPort = this;
    m->setData(score::mime::port(), {});
    d->setMimeData(m);
    d->exec();
    connect(d, &QDrag::destroyed, this, [] { clickedPort = nullptr; });
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
  m_diam = 6.;
  update();
  event->accept();
}

void PortItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
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
  m_diam = 6.;
  update();
  event->accept();
}

QVariant PortItem::itemChange(
    QGraphicsItem::GraphicsItemChange change, const QVariant& value)
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
