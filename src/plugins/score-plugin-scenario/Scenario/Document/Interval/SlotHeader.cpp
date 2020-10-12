#include "SlotHeader.hpp"

#include <Automation/AutomationPresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <QDrag>
#include <QGraphicsView>
#include <QMimeData>
#include <QPainter>
#include <QWidget>

#include <wobjectimpl.h>
namespace Scenario
{
SlotHeader::SlotHeader(const IntervalPresenter& slotView, int slotIndex, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{slotView}
    , m_width{slotView.view()->boundingRect().width()}
    , m_slotIndex{slotIndex}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptHoverEvents(true);
  this->setFlag(ItemClipsToShape);
  this->setFlag(ItemClipsChildrenToShape);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

int SlotHeader::slotIndex() const
{
  return m_slotIndex;
}

void SlotHeader::setSlotIndex(int v)
{
  m_slotIndex = v;
}

QRectF SlotHeader::boundingRect() const
{
  return {0., 0., m_width, headerHeight()};
}

void SlotHeader::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  const auto& style = Process::Style::instance();

  auto& brush = m_presenter.model().metadata().getColor().getBrush().darker300.brush;
  painter->fillRect(QRectF{0., 0., m_width, headerHeight() - 1}, brush);
  if (m_width > 20)
  {
    painter->setPen(style.SlotHeaderPen());
    painter->setBrush(style.NoBrush());
    // Grip
    double r = 4.5;
    double centerX = 9.;
    const double centerY = 7.5;

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->drawLine(centerX - r, centerY + 0, centerX + r, centerY + 0);
    painter->drawLine(centerX - r, centerY - 3, centerX + r, centerY - 3);
    painter->drawLine(centerX - r, centerY + 3, centerX + r, centerY + 3);

    painter->setRenderHint(QPainter::Antialiasing, true);
  }
}

void SlotHeader::setWidth(qreal width)
{
  prepareGeometryChange();
  m_width = width;
  update();
}

static std::unique_ptr<QDrag> slot_header_drag = nullptr;
static bool slot_drag_moving = false;
void SlotHeader::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  slot_header_drag.reset();
  slot_drag_moving = false;
  m_presenter.selectedSlot(m_slotIndex);

  const auto xpos = event->pos().x();
  if (xpos >= 0 && xpos < 16)
  {
    slot_header_drag.reset(new QDrag(event->widget()));
  }
  else if (boundingRect().contains(event->pos()))
  {
    auto slt = m_presenter.getSlots()[m_slotIndex].getLayerSlot();
    if(slt && slt->layers.size() > 1)
    {
      if (const auto tip = dynamic_cast<const TemporalIntervalPresenter*>(&m_presenter))
        tip->requestProcessSelectorMenu(m_slotIndex, event->screenPos(), event->scenePos());
    }
  }
  event->accept();
}

QByteArray SlotHeader::dragMimeData(bool temporal)
{
  std::optional<Id<Process::ProcessModel>> proc_id;
  if(temporal)
  {
    proc_id = m_presenter.model().smallView()[m_slotIndex].frontProcess;
  }
  else
  {
    auto& slt = m_presenter.model().fullView()[m_slotIndex];
    if(!slt.nodal)
      proc_id = slt.process;
  }

  JSONReader r;
  if(proc_id)
  {
    auto& proc = m_presenter.model().processes.at(*proc_id);
    r.stream.StartObject();
    copyProcess(r, proc);
    r.obj["Path"] = score::IDocument::path(proc);
    r.obj["Duration"] = m_presenter.model().duration.defaultDuration().msec();
    r.obj["SlotIndex"] = m_slotIndex;
    r.obj["View"] = temporal ? QStringLiteral("Small") : QStringLiteral("Full");
    r.stream.EndObject();
  }
  else
  {
    r.stream.StartObject();
    r.obj["SlotIndex"] = m_slotIndex;
    r.obj["View"] = temporal ? QStringLiteral("Small") : QStringLiteral("Full");
    r.stream.EndObject();
  }

  return r.toByteArray();
}

void SlotHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();

  const auto xpos = event->buttonDownPos(Qt::LeftButton).x();
  if (xpos >= 0 && xpos < 16 && slot_header_drag)
  {
    auto min_dist
        = (event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength()
          >= QApplication::startDragDistance();

    if (min_dist)
    {
      slot_drag_moving = true;
    }
    if (!slot_drag_moving)
    {
      return;
    }

    bool temporal = dynamic_cast<const TemporalIntervalPresenter*>(&m_presenter);
    QMimeData* mime = new QMimeData;
    mime->setData(score::mime::layerdata(), dragMimeData(temporal));
    slot_header_drag->setMimeData(mime);

    if(auto slt = m_presenter.getSlots()[m_slotIndex].getLayerSlot())
    {
      if (!slt->layers.empty())
      {
        auto& view = slt->layers.front();
        slot_header_drag->setPixmap(view.pixmap().scaledToWidth(50));
        slot_header_drag->setHotSpot(QPoint(5, 5));
      }
    }

    QObject::connect(slot_header_drag.get(), &QDrag::destroyed, &m_presenter, [p = &m_presenter] {
      p->stopSlotDrag();
    });

    m_presenter.startSlotDrag(m_slotIndex, mapToParent(event->pos()));
    slot_header_drag->exec();
    auto ptr = slot_header_drag.release();
    ptr->deleteLater();
  }
}

void SlotHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  slot_header_drag.reset();
  slot_drag_moving = false;
  event->accept();
}

void SlotHeader::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  const auto xpos = event->pos().x();
  if (xpos >= 0 && xpos < 16)
  {
    if (this->cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);
  }
  else
  {
    if (this->cursor().shape() == Qt::CrossCursor)
      unsetCursor();
  }
}

void SlotHeader::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  const auto xpos = event->pos().x();
  if (xpos >= 0 && xpos < 16)
  {
    if (this->cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);
  }
  else
  {
    if (this->cursor().shape() == Qt::CrossCursor)
      unsetCursor();
  }
}

void SlotHeader::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  if (this->cursor().shape() == Qt::CrossCursor)
    unsetCursor();
}

SlotFooter::SlotFooter(const IntervalPresenter& slotView, int slotIndex, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{slotView}
    , m_width{slotView.view()->boundingRect().width()}
    , m_slotIndex{slotIndex}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptHoverEvents(true);
  this->setFlag(ItemClipsToShape);
  this->setFlag(ItemClipsChildrenToShape);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setZValue(ZPos::Header);
}

int SlotFooter::slotIndex() const
{
  return m_slotIndex;
}

void SlotFooter::setSlotIndex(int v)
{
  m_slotIndex = v;
}

QRectF SlotFooter::boundingRect() const
{
  return {0., 0., m_width, footerHeight()};
}

void SlotFooter::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);

  auto& brush = m_presenter.model().metadata().getColor().getBrush();
  painter->fillRect(QRectF{0., 0., m_width, footerHeight() - 1.}, brush.darker300.brush);
  painter->fillRect(QRectF{0., footerHeight() - 1., m_width, 1.}, brush.lighter180.brush);
}

void SlotFooter::setWidth(qreal width)
{
  prepareGeometryChange();
  m_width = width;
  update();
}

AmovibleSlotFooter::AmovibleSlotFooter(
    const IntervalPresenter& slotView,
    int slotIndex,
    QGraphicsItem* parent)
    : SlotFooter{slotView, slotIndex, parent}
    , m_fullView{bool(qobject_cast<const FullViewIntervalPresenter*>(&m_presenter))}
{
  auto& skin = score::Skin::instance();
  this->setCursor(skin.CursorScaleV);
}

void AmovibleSlotFooter::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_presenter.model().smallViewVisible() || m_fullView)
    m_presenter.pressed(event->scenePos());
  event->accept();
}

void AmovibleSlotFooter::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_presenter.model().smallViewVisible() || m_fullView)
  {
    static bool moving = false;

    if (!moving)
    {
      moving = true;
      auto p = event->scenePos();
      m_presenter.moved(p);

      auto view = getView(*this);
      if (view)
        view->ensureVisible(p.x(), p.y(), 1, 1);
      moving = false;
    }
  }
  event->accept();
}

void AmovibleSlotFooter::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_presenter.model().smallViewVisible() || m_fullView)
    m_presenter.released(event->scenePos());

  event->accept();
}

void FixedSlotFooter::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void FixedSlotFooter::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void FixedSlotFooter::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

SlotDragOverlay::SlotDragOverlay(const IntervalPresenter& c, Slot::RackView v)
    : interval{c}, view{v}
{
  this->setAcceptDrops(true);
  this->setZValue(9999);
}

QRectF SlotDragOverlay::boundingRect() const
{
  return interval.view()->boundingRect();
}

void SlotDragOverlay::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  const auto& style = Process::Style::instance();
  auto c = style.IntervalBase().color();
  c.setAlphaF(0.2);
  painter->fillRect(interval.view()->boundingRect(), c);
  painter->fillRect(m_drawnRect, QColor::fromRgbF(0.6, 0.6, 1., 0.8));
  painter->setPen(QPen(QColor::fromRgbF(0.6, 0.6, 1., 0.7), 2));
  painter->drawRect(m_drawnRect);
}

void SlotDragOverlay::onDrag(QPointF pos)
{
  const auto y = pos.y();
  const auto& itv = interval.model();
  const auto rect = interval.view()->boundingRect();

  double height = 0.;

  if (y <= height)
  {
    m_drawnRect = {0, height - 2.5, rect.width(), 5};
    update();
  }
  else if (y > rect.height() - 5.)
  {
    m_drawnRect = {0, rect.height() - 5., rect.width(), 5};
    update();
  }
  else
  {
    const int N = int(view == Slot::SmallView ? itv.smallView().size() : itv.fullView().size());
    for (int i = 0; i < N; i++)
    {
      const auto next_height
          = itv.getSlotHeight({i, view}) + SlotHeader::headerHeight() + SlotFooter::footerHeight();
      if (y > height - 2.5 && y < height + 2.5)
      {
        m_drawnRect = {0, height - 2.5, rect.width(), 5};
        update();
        break;
      }
      else if (y < height + next_height - 2.5)
      {
        m_drawnRect = {0, height, rect.width(), next_height};
        update();
        break;
      }

      height += next_height;
    }

    if (y > height - 2.5 && y < height + 2.5)
    {
      m_drawnRect = {0, height - 2.5, rect.width(), 5};
      update();
    }
  }
}

void SlotDragOverlay::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  m_drawnRect = {};

  onDrag(event->pos());
  event->accept();
}

void SlotDragOverlay::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  onDrag(event->pos());
  event->accept();
}

void SlotDragOverlay::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  m_drawnRect = {};
  update();
  event->accept();
}

void SlotDragOverlay::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
  const auto pos = event->pos();
  const auto y = pos.y();
  const auto rect = interval.view()->boundingRect();
  const auto& itv = interval.model();
  double height = 0.;
  const int N = int(view == Slot::SmallView ? itv.smallView().size() : itv.fullView().size());

  if (y <= height)
  {
    m_drawnRect = {0, height - 2.5, rect.width(), 5};
    dropBefore(0);
    update();
  }
  else if (y > rect.height() - 5.)
  {
    m_drawnRect = {0, rect.height() - 5., rect.width(), 5};
    dropBefore(N);
    update();
  }
  else
  {
    for (int i = 0; i < N; i++)
    {
      const auto next_height
          = itv.getSlotHeight({i, view}) + SlotHeader::headerHeight() + SlotFooter::footerHeight();
      if (y > height - 2.5 && y < height + 2.5)
      {
        m_drawnRect = {0, height - 2.5, rect.width(), 5};
        dropBefore(i);
        update();
        return;
      }
      else if (y < height + next_height - 2.5)
      {
        m_drawnRect = {0, height, rect.width(), next_height};
        dropIn(i);
        update();
        return;
      }

      height += next_height;
    }

    if (y > height - 2.5 && y < height + 2.5)
    {
      m_drawnRect = {0, height - 2.5, rect.width(), 5};
      dropBefore(N);
      update();
      return;
    }
  }
}
}

W_OBJECT_IMPL(Scenario::SlotDragOverlay)
