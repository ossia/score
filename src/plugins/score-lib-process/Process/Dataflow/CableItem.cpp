#include "CableItem.hpp"

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/PainterPath.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::CableItem)
namespace Dataflow
{
bool CableItem::g_cables_enabled = true;

static bool canCreateCable(const Process::Cable& c, Process::DataflowManager& plug)
{
  auto it = plug.cables().find(&c);
  return it == plug.cables().end() || it->second == nullptr;
}
CableItem::CableItem(
    const Process::Cable& c, const Process::Context& ctx, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_cable{c}
    , m_context{ctx}
{
  auto& plug = ctx.dataflow;
  this->setCursor(Qt::CrossCursor);
  this->setFlag(QGraphicsItem::ItemClipsToShape);
  this->setFlag(QGraphicsItem::ItemIsFocusable);
  this->setAcceptDrops(true);
  this->setToolTip(tr("Cable\n"));

  SCORE_ASSERT(canCreateCable(c, plug));

  con(c.selection, &Selectable::changed, this, [this](bool b) {
    if(m_p1 && m_p2)
    {
      if(b)
      {
        setZValue(999999);
        m_p1->setHighlight(true);
        m_p2->setHighlight(true);
        this->setFocus(Qt::OtherFocusReason);
      }
      else
      {
        setZValue(-1);
        m_p1->setHighlight(false);
        m_p2->setHighlight(false);
        this->clearFocus();
      }
      update();
    }
  });

  auto& p = plug.ports();
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
    ossia::remove_erase(m_p1->cables, this);
  }
  if(m_p2)
  {
    ossia::remove_erase(m_p2->cables, this);
  }
  auto& plug = m_context.dataflow;
  auto& p = plug.cables();
  auto it = p.find(&m_cable);
  if(it != p.end())
    it->second = nullptr;
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

void CableItem::updateStroke() const
{
  if(!m_stroke.isEmpty())
    return;
  m_stroke = cableStroker().createStroke(m_path);
}

QRectF CableItem::boundingRect() const
{
  updateStroke();
  return m_stroke.boundingRect();
}

bool CableItem::contains(const QPointF& point) const
{
  if(m_mode == None)
    return false;
  updateStroke();
  return m_stroke.contains(point);
}

void CableItem::setPen(QPainter& painter, const Process::Style& style)
{
  switch(m_mode)
  {
    case Full: {
      if(m_dropping)
      {
        [[unlikely]];
        switch(m_type)
        {
          case Process::PortType::Message:
            painter.setPen(style.DragDropDataCablePen());
            break;
          case Process::PortType::Audio:
            painter.setPen(style.DragDropAudioCablePen());
            break;
          case Process::PortType::Midi:
            painter.setPen(style.DragDropMidiCablePen());
            break;
          case Process::PortType::Texture:
            painter.setPen(style.DragDropTextureCablePen());
            break;
          case Process::PortType::Geometry:
            painter.setPen(style.DragDropGeometryCablePen());
            break;
        }
      }
      else if(!m_cable.selection.get())
      {
        [[likely]]
        switch(m_type)
        {
          case Process::PortType::Message:
            painter.setPen(style.DataCablePen());
            break;
          case Process::PortType::Audio:
            painter.setPen(style.AudioCablePen());
            break;
          case Process::PortType::Midi:
            painter.setPen(style.MidiCablePen());
            break;
          case Process::PortType::Texture:
            painter.setPen(style.TextureCablePen());
            break;
          case Process::PortType::Geometry:
            painter.setPen(style.GeometryCablePen());
            break;
        }
      }
      else
      {
        switch(m_type)
        {
          case Process::PortType::Message:
            painter.setPen(style.SelectedDataCablePen());
            break;
          case Process::PortType::Audio:
            painter.setPen(style.SelectedAudioCablePen());
            break;
          case Process::PortType::Midi:
            painter.setPen(style.SelectedMidiCablePen());
            break;
          case Process::PortType::Texture:
            painter.setPen(style.SelectedTextureCablePen());
            break;
          case Process::PortType::Geometry:
            painter.setPen(style.SelectedGeometryCablePen());
            break;
        }
      }
      break;
    }
    case Partial_P1:
    case Partial_P2: {
      if(!m_cable.selection.get())
      {
        [[likely]]
        switch(m_type)
        {
          case Process::PortType::Message:
            painter.setPen(style.skin.Cable2.main.pen1_dotted);
            break;
          case Process::PortType::Audio:
            painter.setPen(style.skin.Cable1.main.pen1_dotted);
            break;
          case Process::PortType::Midi:
            painter.setPen(style.skin.Cable3.main.pen1_dotted);
            break;
          case Process::PortType::Texture:
            painter.setPen(style.skin.LightGray.main.pen1_dotted);
            break;
          case Process::PortType::Geometry:
            painter.setPen(style.skin.Emphasis3.main.pen1_dotted);
            break;
        }
      }
      else
      {
        switch(m_type)
        {
          case Process::PortType::Message:
            painter.setPen(style.skin.SelectedCable2.lighter.pen1_dotted);
            break;
          case Process::PortType::Audio:
            painter.setPen(style.skin.SelectedCable1.lighter.pen1_dotted);
            break;
          case Process::PortType::Midi:
            painter.setPen(style.skin.SelectedCable3.lighter.pen1_dotted);
            break;
          case Process::PortType::Texture:
            painter.setPen(style.skin.LightGray.lighter.pen1_dotted);
            break;
          case Process::PortType::Geometry:
            painter.setPen(style.skin.Emphasis3.lighter.pen1_dotted);
            break;
        }
      }
      break;
    }
    default:
      break;
  }
}

void CableItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(m_p1 && m_p2 && m_mode != CableDisplayMode::None)
  {
    auto& style = Process::Style::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(style.TransparentBrush());
    setPen(*painter, style);
    painter->drawPath(m_path);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
}

void CableItem::resize()
{
  prepareGeometryChange();

  m_stroke.clear();
  m_path.clear();
  if(m_p1 && m_p2)
  {
    auto p1 = m_p1->scenePos() + QPointF(6., 6.) * m_p1->sceneTransform().m11();
    auto p2 = m_p2->scenePos() + QPointF(6., 6.) * m_p2->sceneTransform().m11();

    this->setPos(QRectF{p1, p2}.normalized().topLeft());

    p1 = mapFromScene(p1);
    p2 = mapFromScene(p2);
    bool x_dir = p1.x() > p2.x();

    if(m_mode == Full)
    {
      auto first = x_dir ? p1 : p2;
      auto last = !x_dir ? p1 : p2;

      int half_length = std::floor(0.5 * (last.x() - first.x()));

      auto y_direction = last.y() > first.y() ? 1 : -1;
      auto offset_y = y_direction * half_length / 10.f;

      m_path.moveTo(first.x(), first.y());
      m_path.cubicTo(
          first.x() + half_length, first.y() + offset_y, last.x() - half_length,
          last.y() - offset_y, last.x(), last.y());
    }
    else if(m_mode == Partial_P1)
    {
      auto dir = (p2 - p1) / QLineF(p1, p2).length();
      m_path.moveTo(p1.x(), p1.y());
      m_path.lineTo(p1 + 50. * dir);
    }
    else if(m_mode == Partial_P2)
    {
      auto dir = (p1 - p2) / QLineF(p1, p2).length();
      m_path.moveTo(p2.x(), p2.y());
      m_path.lineTo(p2 + 50. * dir);
    }
  }

  update();
}

static bool isPortActuallyVisible(QGraphicsItem* port)
{
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

static CableDisplayMode cableMustBeShown(CableItem& self, PortItem* p1, PortItem* p2)
{
  if(!p1 || !p2)
    return None;

  {
    const bool p1_vis = p1->isVisible();
    const bool p2_vis = p2->isVisible();
    if(p1_vis && !p2_vis)
      return Partial_P1;
    else if(!p1_vis && p2_vis)
      return Partial_P2;
    else if(!p1_vis && !p2_vis)
      return None;
  }

  auto proc_p1 = Process::parentProcess(&p1->port());
  auto proc_p2 = Process::parentProcess(&p2->port());

  if(proc_p1 && proc_p2 && proc_p1->parent() == proc_p2->parent())
  {
    if(self.parentItem()
       && self.parentItem()->type() == 65554) // We're in full nodal view
      return Full;

    // Ideal heuritstic: find the closest {interval, nodalcontainer} item for p1 and p2.
    // But too slow so we approximate - there is a potential issue if both are in different
    // intervals themselves in a scenario, itself in a single nodal view
    auto ancestor = p1->commonAncestorItem(p2);
    if(ancestor
       && ancestor->type()
              == (QGraphicsItem::UserType + 5555)) // we're in the same NodalContainer
      return Full;
  }

  {
    const bool p1_vis = isPortActuallyVisible(p1);
    const bool p2_vis = isPortActuallyVisible(p2);
    if(p1_vis && p2_vis)
      return Full;
    else if(p1_vis && !p2_vis)
      return Partial_P1;
    else if(!p1_vis && p2_vis)
      return Partial_P2;
    else
      return None;
  }
}

void CableItem::check()
{
  if(g_cables_enabled)
  {
    m_mode = cableMustBeShown(*this, m_p1, m_p2);
    if(m_mode != CableDisplayMode::None)
    {
      if(!isEnabled())
      {
        setVisible(true);
        setEnabled(true);
      }
      else if(!isVisible())
      {
        setVisible(true);
      }
      m_type = m_p1->port().type();
      if(auto c_o = m_p1->commonAncestorItem(m_p2))
        setParentItem(c_o);
      resize();
    }
  }
  else if(isEnabled())
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
  if(m_mode == None)
    return {};
  updateStroke();
  return m_stroke;
}

QPainterPath CableItem::opaqueArea() const
{
  if(m_mode == None)
    return {};
  updateStroke();
  return m_stroke;
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

void CableItem::keyPressEvent(QKeyEvent* event)
{
  switch(event->key())
  {
    case Qt::Key_Left:
    case Qt::Key_Up: {
      auto& source = this->m_cable.source().find(this->m_context);
      this->m_context.selectionStack.pushNewSelection({&source});
      break;
    }
    case Qt::Key_Right:
    case Qt::Key_Down: {
      auto& sink = this->m_cable.sink().find(this->m_context);
      this->m_context.selectionStack.pushNewSelection({&sink});
      break;
    }
  }
  event->accept();
}

void CableItem::keyReleaseEvent(QKeyEvent* event)
{
  event->accept();
}

void CableItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropping = true;
  event->accept();
  update();
}

void CableItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropping = false;
  event->accept();
  update();
}

void CableItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropping = false;
  dropReceived(event->pos(), *event->mimeData());
  event->accept();
  update();
}
}
