#include "CableItem.hpp"

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/PainterPath.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <tsl/hopscotch_map.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::CableItem)
namespace Dataflow
{
bool CableItem::g_cables_enabled = true;

static bool
canCreateCable(const Process::Cable& c, Process::DataflowManager& plug)
{
  auto it = plug.cables().find(&c);
  return it == plug.cables().end() || it->second == nullptr;
}
CableItem::CableItem(
    const Process::Cable& c,
    const Process::Context& ctx,
    QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_cable{c}
    , m_context{ctx}
{
  auto& plug = ctx.dataflow;
  this->setCursor(Qt::CrossCursor);
  this->setFlag(QGraphicsItem::ItemClipsToShape);

  SCORE_ASSERT(canCreateCable(c, plug));

  con(c.selection, &Selectable::changed, this, [=](bool b) {
    if (m_p1 && m_p2)
    {
      if (b)
      {
        setZValue(999999);
        m_p1->setHighlight(true);
        m_p2->setHighlight(true);
      }
      else
      {
        setZValue(-1);
        m_p1->setHighlight(false);
        m_p2->setHighlight(false);
      }
      update();
    }
  });

  auto& p = plug.ports();
  if (auto src_port = c.source().try_find(ctx))
  {
    auto src = p.find(src_port);
    if (src != p.end())
    {
      m_p1 = src->second;
      m_p1->cables.push_back(this);
    }
  }

  if (auto snk_port = c.sink().try_find(ctx))
  {
    auto snk = p.find(snk_port);
    if (snk != p.end())
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
  if (m_p1)
  {
    ossia::remove_erase(m_p1->cables, this);
  }
  if (m_p2)
  {
    ossia::remove_erase(m_p2->cables, this);
  }
  auto& plug = m_context.dataflow;
  auto& p = plug.cables();
  auto it = p.find(&m_cable);
  if (it != p.end())
    it.value() = nullptr;
}

static const QPainterPathStroker& cableStroker()
{
  static const QPainterPathStroker cable_stroker{[] {
    QPen pen;
    pen.setCapStyle(Qt::PenCapStyle::RoundCap);
    pen.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
    pen.setWidthF(7.);
    return pen;
  }()};
  return cable_stroker;
}
QRectF CableItem::boundingRect() const
{
  return cableStroker().createStroke(m_path).boundingRect();
}

bool CableItem::contains(const QPointF& point) const
{
  return cableStroker().createStroke(m_path).contains(point);
  // return m_path.contains(point);
}

void CableItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  if (m_p1 && m_p2)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    auto& style = Process::Style::instance();
    if (!m_cable.selection.get())
    {
      switch (m_type)
      {
        case Process::PortType::Message:
          painter->setPen(style.DataCablePen());
          break;
        case Process::PortType::Audio:
          painter->setPen(style.AudioCablePen());
          break;
        case Process::PortType::Midi:
          painter->setPen(style.MidiCablePen());
          break;
        case Process::PortType::Texture:
          painter->setPen(style.skin.LightGray.main.pen3_solid_round_round);
          break;
      }
    }
    else
    {
      switch (m_type)
      {
        case Process::PortType::Message:
          painter->setPen(style.SelectedDataCablePen());
          break;
        case Process::PortType::Audio:
          painter->setPen(style.SelectedAudioCablePen());
          break;
        case Process::PortType::Midi:
          painter->setPen(style.SelectedMidiCablePen());
          break;
        case Process::PortType::Texture:
          painter->setPen(style.skin.LightGray.lighter.pen3_solid_round_round);
          break;
      }
    }

    painter->setBrush(style.TransparentBrush());
    painter->drawPath(m_path);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
}

void CableItem::resize()
{
  prepareGeometryChange();

  clearPainterPath(m_path);
  if (m_p1 && m_p2)
  {
    auto p1 = m_p1->scenePos() + QPointF(6., 6.);
    auto p2 = m_p2->scenePos() + QPointF(6., 6.);

    auto rect = QRectF{p1, p2};
    auto nrect = rect.normalized();
    this->setPos(nrect.topLeft());
    nrect.translate(-nrect.topLeft().x(), -nrect.topLeft().y());

    p1 = mapFromScene(p1);
    p2 = mapFromScene(p2);

    bool x_dir = p1.x() > p2.x();
    auto first = x_dir ? p1 : p2;
    auto last = !x_dir ? p1 : p2;

    int half_length = std::floor(0.5 * (last.x() - first.x()));

    auto y_direction = last.y() > first.y() ? 1 : -1;
    auto offset_y = y_direction * half_length / 10.f;

    m_path.moveTo(first.x(), first.y());
    m_path.cubicTo(
        first.x() + half_length,
        first.y() + offset_y,
        last.x() - half_length,
        last.y() - offset_y,
        last.x(),
        last.y());
  }

  update();
}

bool isPortActuallyVisible(QGraphicsItem* port)
{
  if(!port)
    return false;
  if(!port->isVisible())
    return false;

  if(QGraphicsItem* parent = port->parentItem())
  {
    do
    {
      const auto parentRect = parent->boundingRect();

      // Case of the empty rect
      if(parentRect.width() == 0.0 && parentRect.height() == 0.0)
        continue;

      const auto point = port->mapToItem(parent, QPointF{5., 5.});
      if(!parentRect.contains(point))
      {
        return false;
      }

    } while((parent = parent->parentItem()));
  }
  return true;
}

void CableItem::check()
{
  if (g_cables_enabled && isPortActuallyVisible(m_p1) && isPortActuallyVisible(m_p2))
  {
    if (!isEnabled())
    {
      setVisible(true);
      setEnabled(true);
    }
    else if (!isVisible())
    {
      setVisible(true);
    }
    m_type = m_p1->port().type();
    if (auto c_o = m_p1->commonAncestorItem(m_p2))
      setParentItem(c_o);
    resize();
  }
  else if (isEnabled())
  {
    setVisible(false);
    setEnabled(false);
    update();
  }
}

PortItem* CableItem::source() const noexcept
{
  return m_p1;
}

PortItem* CableItem::target() const noexcept
{
  return m_p2;
}

void CableItem::setSource(PortItem* p)
{
  m_p1 = p;
  check();
}

void CableItem::setTarget(PortItem* p)
{
  m_p2 = p;
  check();
}

QPainterPath CableItem::shape() const
{
  return cableStroker().createStroke(m_path);
}

QPainterPath CableItem::opaqueArea() const
{
  return cableStroker().createStroke(m_path);
}

void CableItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(!m_p1 || !m_p2)
  {
    event->ignore();
    return;
  }

  score::SelectionDispatcher disp{m_context.selectionStack};
  if(m_p1->contains(m_p1->mapFromScene(event->scenePos())))
    disp.select(m_p1->port());
  else if(m_p2->contains(m_p2->mapFromScene(event->scenePos())))
    disp.select(m_p2->port());
  else
    disp.select(m_cable);
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


}
