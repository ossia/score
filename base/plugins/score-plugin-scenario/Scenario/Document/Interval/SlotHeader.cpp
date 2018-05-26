#include "SlotHeader.hpp"

#include <Automation/AutomationPresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QDrag>
#include <QGraphicsSceneEvent>
#include <QGraphicsView>
#include <QMimeData>
#include <QWidget>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>

namespace Scenario
{
SlotHeader::SlotHeader(
    const IntervalPresenter& slotView, int slotIndex, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{slotView}
    , m_width{slotView.view()->boundingRect().width()}
    , m_slotIndex{slotIndex}
{
  this->setCacheMode(QGraphicsItem::NoCache);
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

void SlotHeader::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  const auto& style = ScenarioStyle::instance();
  if (m_width > 20)
  {
    painter->setPen(style.IntervalHeaderSeparator);
    painter->setBrush(style.NoBrush);

    // Grip
    static const std::array<QRectF, 6> rects{[] {
      std::array<QRectF, 6> rects;
      for (int i = 0; i < 3; i++)
        rects[i] = {6., 5 + 3. * i, 0.1, 0.1};
      return rects;
    }()};
    painter->drawRects(rects.data(), 6);

    painter->setRenderHint(QPainter::Antialiasing, true);

    // Frame
    painter->drawRect(QRectF{0., 0., m_width, headerHeight() - 1});

    // Menu
    {
      auto view = getView(*this);
      if (!view)
        return;

      double r = 4.5;

      const double leftXinView
          = view->mapFromScene(mapToScene(QPointF{(m_width - 8.) - r, 0.}))
                .x();
      const double rightXinView = leftXinView + 2. * r;

      double centerX = m_width - 8.;
      const double centerY = 7.5;

      const constexpr double min_x = 10.;
      const double max_x = view->width() - 30.;
      if (leftXinView <= min_x)
      {
        centerX += (-leftXinView + min_x);
      }
      else if (rightXinView >= max_x)
      {
        centerX += (-rightXinView + max_x);
      }
      centerX = std::max(centerX, 5.);

      painter->setBrush(style.MinimapBrush);
      painter->drawEllipse(QPointF{centerX, centerY}, r, r);
      r -= 1.;
      painter->setRenderHint(QPainter::Antialiasing, false);
      painter->setPen(style.TimeRulerSmallPen);
      painter->drawLine(
          QPointF{centerX, centerY - r}, QPointF{centerX, centerY + r});
      painter->drawLine(
          QPointF{centerX - r, centerY}, QPointF{centerX + r, centerY});
      m_menupos = centerX;
    }
  }
  else
  {
    painter->setPen(style.IntervalHeaderSeparator);
    painter->setBrush(style.NoBrush);

    painter->drawRect(QRectF{0., 0., m_width, headerHeight() - 1});
  }
}

void SlotHeader::setWidth(qreal width)
{
  prepareGeometryChange();
  m_width = width;
  update();
}

void SlotHeader::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.selectedSlot(m_slotIndex);

  const auto xpos = event->pos().x();
  if (xpos >= 0 && xpos < 16)
  {
  }
  else if (xpos >= m_menupos - 4 && xpos < m_menupos + 4)
  {
    // menu
    if (const auto tip
        = dynamic_cast<const TemporalIntervalPresenter*>(&m_presenter))
      tip->requestSlotMenu(m_slotIndex, event->screenPos(), event->scenePos());
  }
  else if(boundingRect().contains(event->pos()))
  {
    if (const auto tip
        = dynamic_cast<const TemporalIntervalPresenter*>(&m_presenter))
      tip->requestProcessSelectorMenu(
          m_slotIndex, event->screenPos(), event->scenePos());
  }
  event->accept();
}

void SlotHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();

  const auto xpos = event->pos().x();
  if (xpos >= 0 && xpos < 16)
  {
    if ((event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton))
            .manhattanLength() < QApplication::startDragDistance())
    {
      return;
    }

    QDrag* drag = new QDrag(event->widget());
    QMimeData* mime = new QMimeData;
    drag->setMimeData(mime);

    mime->setData(score::mime::layerdata(), {});

    auto view = m_presenter.getSlots()[m_slotIndex].processes.front().view;
    drag->setPixmap(view->pixmap());
    drag->setHotSpot(QPoint(5, 5));

    m_presenter.startSlotDrag(m_slotIndex, mapToParent(event->pos()));
    drag->exec();
    m_presenter.stopSlotDrag();
  }
}

void SlotHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
}
