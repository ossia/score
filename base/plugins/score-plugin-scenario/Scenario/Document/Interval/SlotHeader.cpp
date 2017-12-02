#include "SlotHeader.hpp"
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QGraphicsSceneEvent>
#include <QDrag>
#include <QMimeData>

namespace Scenario
{
SlotHeader::SlotHeader(
    const IntervalPresenter& slotView,
    int slotIndex,
    QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{slotView}
    , m_width{slotView.view()->boundingRect().width()}
    , m_slotIndex{slotIndex}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setFlag(ItemClipsToShape);
  this->setFlag(ItemClipsChildrenToShape);
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
  const auto& style = ScenarioStyle::instance();
  painter->setPen(style.IntervalHeaderSeparator);
  painter->setBrush(style.NoBrush);

  // Grip
  painter->setRenderHint(QPainter::Antialiasing, false);
  static const std::array<QRectF, 6> rects{ [] {
    std::array<QRectF, 6> rects;
    double x = 4;
    for(int i = 0; i < 6; i++)
      rects[i] = {x += 2, (i % 2 == 0 ? 9. : 5.), 0.1, 0.1};
    return rects;
  }() };
  painter->drawRects(rects.data(), 6);

  painter->setRenderHint(QPainter::Antialiasing, true);

  // Frame
  painter->drawRect(QRectF{0., 0., m_width, headerHeight() - 1});

  // Menu
  double centerX = m_width - 8.;
  double centerY = 7.5;
  double r = 4.5;
  painter->setBrush(style.MinimapBrush);
  painter->drawEllipse(QPointF{centerX, centerY}, r, r);
  r -= 1.;
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setPen(style.TimeRulerSmallPen);
  painter->drawLine(QPointF{centerX, centerY - r}, QPointF{centerX, centerY + r});
  painter->drawLine(QPointF{centerX - r, centerY }, QPointF{centerX + r, centerY });
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
  if(xpos >= 0 && xpos < 16)
  {
  }
  else if(xpos >= m_width - 16)
  {
    // menu
    if ( const auto tip = dynamic_cast<const TemporalIntervalPresenter*> (&m_presenter))
      tip->requestSlotMenu(m_slotIndex, event->screenPos(), event->scenePos());
  }
  event->accept();
}


void SlotHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();

  const auto xpos = event->pos().x();
  if(xpos >= 0 && xpos < 16)
  {
    if (QLineF(event->screenPos(), event->buttonDownScreenPos(Qt::LeftButton))
        .length() < QApplication::startDragDistance()) {
        return;
    }

    QDrag *drag = new QDrag(event->widget());
    QMimeData *mime = new QMimeData;
    drag->setMimeData(mime);

    mime->setData("application/x-score-layerdrag", {});
    /*
    drag->setPixmap(QPixmap::fromImage(image).scaled(30, 40));
    drag->setHotSpot(QPoint(15, 30));
    */

    drag->exec();
  }
}

void SlotHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
}
