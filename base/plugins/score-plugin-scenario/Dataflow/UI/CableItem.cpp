#include "CableItem.hpp"
#include <Dataflow/UI/PortItem.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <QGraphicsSceneMoveEvent>
#include <boost/range/algorithm_ext/erase.hpp>

#include <QMenu>
#include <QPainter>

namespace Dataflow
{
CableItem::cable_map CableItem::g_cables;

CableItem::CableItem(Process::Cable& c, QGraphicsItem* parent):
  QGraphicsItem{parent}
, m_cable{c}
{
  this->setCursor(Qt::CrossCursor);
  g_cables.insert({&c, this});

  auto src = PortItem::g_ports.find(c.source());
  if(src != PortItem::g_ports.end())
  {
    m_p1 = src->second;
    m_p1->cables.push_back(this);
  }
  auto snk = PortItem::g_ports.find(c.sink());
  if(snk != PortItem::g_ports.end())
  {
    m_p2 = snk->second;
    m_p2->cables.push_back(this);
  }
  check();
  resize();
}

CableItem::~CableItem()
{
  if(m_p1)
  {
    boost::remove_erase(m_p1->cables, this);
  }
  if(m_p2)
  {
    boost::remove_erase(m_p2->cables, this);
  }

  auto it = g_cables.find(&m_cable);
  if(it != g_cables.end())
    g_cables.erase(it);
}

QRectF CableItem::boundingRect() const
{
  return m_path.boundingRect();
}

void CableItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(m_p1 && m_p2)
  {
    static QPen cablepen{[] {
        QPen pen;
        pen.setCapStyle(Qt::PenCapStyle::RoundCap);
        pen.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
        pen.setWidthF(3.);
        return pen;
      }()};

    static const QColor messageColor = QColor("#669966dd");
    static const QColor audioColor = QColor("#669966dd");
    static const QColor midiColor = QColor("#669966dd");
    switch(m_type)
    {
    case Process::PortType::Message:
      cablepen.setColor(messageColor); break;
    case Process::PortType::Audio:
      cablepen.setColor(audioColor); break;
    case Process::PortType::Midi:
      cablepen.setColor(midiColor); break;
    }

    painter->setPen(cablepen);
    painter->setBrush(Qt::transparent);
    painter->drawPath(m_path);
  }
}

void CableItem::resize()
{
  prepareGeometryChange();

  if(m_p1 && m_p2)
  {
    auto p1 = m_p1->scenePos();
    auto p2 = m_p2->scenePos();

    auto rect = QRectF{p1, p2};
    auto nrect = rect.normalized();
    this->setPos(nrect.topLeft());
    nrect.translate(-nrect.topLeft().x(), -nrect.topLeft().y());

    p1 = mapFromScene(p1);
    p2 = mapFromScene(p2);

    auto first = p1.x() < p2.x() ? p1 : p2;
    auto last = p1.x() >= p2.x() ? p1 : p2;
    QPainterPath p;
    p.moveTo(first.x(), first.y());
    p.cubicTo(first.x(), last.y(), first.x(), last.y(), last.x(), last.y());
    m_path = std::move(p);
  }
  else
  {
    m_path = QPainterPath{};
  }

  update();
}

void CableItem::check()
{
  if(m_p1 && m_p2 && m_p1->isVisible() && m_p2->isVisible()) {
    if(!isEnabled())
    {
      setVisible(true);
      setEnabled(true);
      m_type = m_p1->port().type;
    }
    resize();
  }
  else if(isEnabled()) {
    setVisible(false);
    setEnabled(false);
  }
}

PortItem*CableItem::source() const { return m_p1; }

PortItem*CableItem::target() const { return m_p2; }

void CableItem::setSource(PortItem* p) { m_p1 = p; check(); }

void CableItem::setTarget(PortItem* p) { m_p2 = p; check(); }

QPainterPath CableItem::shape() const
{
  static const QPainterPathStroker cable_stroker{[] {
      QPen pen;
      pen.setCapStyle(Qt::PenCapStyle::RoundCap);
      pen.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
      pen.setWidthF(3.);
      return pen;
    }()};

  return cable_stroker.createStroke(m_path);
}

void CableItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  emit clicked();
  event->accept();
}

void CableItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void CableItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void CableItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  QMenu* m = new QMenu;
  auto act = m->addAction(tr("Remove"));
  connect(act, &QAction::triggered,
          this, [=] {
    emit removeRequested();
  });
  m->exec();
  m->deleteLater();
}

}
