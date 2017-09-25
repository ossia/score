#include "PortItem.hpp"
#include <Dataflow/UI/CableItem.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <QDrag>
#include <QGraphicsSceneMoveEvent>
#include <QMimeData>

namespace Dataflow
{
PortItem::port_map PortItem::g_ports;
PortItem::PortItem(Process::Port& p, QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_port{p}
{
  this->setCursor(QCursor());
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);

  g_ports.insert({&p, this});

  for(auto c : CableItem::g_cables)
  {
    if(c.first->source() == &p)
    {
      c.second->setSource(this);
      cables.push_back(c.second);
    }
    else if(c.first->sink() == &p)
    {
      c.second->setTarget(this);
      cables.push_back(c.second);
    }
  }
}

PortItem::~PortItem()
{
  for(auto cable : cables)
  {
    if(cable->source() == this)
      cable->setSource(nullptr);
    if(cable->target() == this)
      cable->setTarget(nullptr);
  }
  auto it = g_ports.find(&m_port);
  if(it != g_ports.end())
    g_ports.erase(it);
}

QRectF PortItem::boundingRect() const
{
  return {-m_diam/2., -m_diam/2., m_diam, m_diam};
}

void PortItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  QColor c;
  switch(m_port.type)
  {
    case Process::PortType::Audio:
      c = QColor("#FFAAAA");
      break;
    case Process::PortType::Message:
      c = QColor("#AAFFAA");
      break;
    case Process::PortType::Midi:
      c = QColor("#AAAAFF");
      break;
  }

  QPen p = c;
  p.setWidth(2);
  QBrush b = c.darker();

  painter->setPen(p);
  painter->setBrush(b);
  painter->drawEllipse(boundingRect());
  painter->setRenderHint(QPainter::Antialiasing, false);
}

static PortItem* clickedPort{};
void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  clickedPort = this;
  event->accept();
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(QLineF(pos(), event->pos()).length() > QApplication::startDragDistance())
  {
    QDrag d{this};
    QMimeData* m = new QMimeData;
    m->setText("cable");
    d.setMimeData(m);
    d.exec();
  }
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->contains(event->pos()))
    emit showPanel();
  event->accept();
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

void PortItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  if(this != clickedPort)
  {
    if(this->m_port.outlet != clickedPort->m_port.outlet)
    {
      emit createCable(clickedPort, this);
    }
  }
  clickedPort = nullptr;
  event->accept();
}

QVariant PortItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch(change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      for(auto cbl : cables)
      {
        cbl->resize();
      }
      break;
    case QGraphicsItem::ItemVisibleHasChanged:
    case QGraphicsItem::ItemSceneHasChanged:
      for(auto cbl : cables)
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
