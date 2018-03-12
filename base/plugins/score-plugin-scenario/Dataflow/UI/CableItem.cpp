#include "CableItem.hpp"
#include <Dataflow/UI/PortItem.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <QGraphicsSceneMoveEvent>
#include <Process/Style/ScenarioStyle.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

#include <QMenu>
#include <QPainter>
#include <QSlider>
#include <QFormLayout>
namespace Dataflow
{
bool CableItem::g_cables_enabled = true;

CableItem::CableItem(Process::Cable& c, const score::DocumentContext& ctx, QGraphicsItem* parent):
  QGraphicsItem{parent}
, m_cable{c}
, a1{int8_t(abs(qrand()) % 40 - 20)}
, a2{int8_t(abs(qrand()) % 40 - 20)}
, a3{int8_t(abs(qrand()) % 40 - 20)}
, a4{int8_t(abs(qrand()) % 40 - 20)}
{
  this->setCursor(Qt::CrossCursor);
  g_cables().insert({&c, this});

  con(c.selection, &Selectable::changed, this, [=](bool b) {
    update();
  });

  auto& p = PortItem::g_ports();
  if(auto src_port = c.source().try_find(ctx))
  {
    auto src = p.find(src_port);
    if(src != p.end())
    {
      m_p1 = src->second;
      m_p1->cables.push_back(this);
    }
  }

  if(auto snk_port = c.sink().try_find(ctx))
  {
    auto snk = p.find(snk_port);
    if(snk != p.end())
    {
      m_p2 = snk->second;
      m_p2->cables.push_back(this);
    }
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

  auto& c = g_cables();
  auto it = c.find(&m_cable);
  if(it != c.end())
    c.erase(it);
}

QRectF CableItem::boundingRect() const
{
  return m_path.boundingRect();
}

void CableItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(m_p1 && m_p2)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    auto& style = ScenarioStyle::instance();
    if(!m_cable.selection.get())
    {
      switch(m_type)
      {
        case Process::PortType::Message:
          painter->setPen(style.DataCablePen); break;
        case Process::PortType::Audio:
          painter->setPen(style.AudioCablePen); break;
        case Process::PortType::Midi:
          painter->setPen(style.MidiCablePen); break;
      }
    }
    else
    {
      switch(m_type)
      {
        case Process::PortType::Message:
          painter->setPen(style.SelectedDataCablePen); break;
        case Process::PortType::Audio:
          painter->setPen(style.SelectedAudioCablePen); break;
        case Process::PortType::Midi:
          painter->setPen(style.SelectedMidiCablePen); break;
      }
    }


    painter->setBrush(style.TransparentBrush);
    painter->drawPath(m_path);
    painter->setRenderHint(QPainter::Antialiasing, false);
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
    p.cubicTo(first.x() + a1, last.y() + a2, first.x() +a3 , last.y() + a4, last.x(), last.y());
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
  if(g_cables_enabled && m_p1 && m_p2 && m_p1->isVisible() && m_p2->isVisible()) {

    if(!isEnabled())
    {
      setVisible(true);
      setEnabled(true);
    }
    else if(!isVisible())
    {
      setVisible(true);
    }
    m_type = m_p1->port().type;
    resize();
  }
  else if(isEnabled()) {
    setVisible(false);
    setEnabled(false);
    update();
  }
}

PortItem*CableItem::source() const { return m_p1; }

PortItem*CableItem::target() const { return m_p2; }

void CableItem::setSource(PortItem* p) { m_p1 = p; check(); }

void CableItem::setTarget(PortItem* p) { m_p2 = p; check(); }

CableItem::cable_map&CableItem::g_cables()
{
  static cable_map c;
  return c;
}

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
  clicked();
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
    removeRequested();
  });
  m->exec(event->screenPos());
  m->deleteLater();
}

}
