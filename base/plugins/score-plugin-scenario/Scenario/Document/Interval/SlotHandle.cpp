// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPoint>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <score/widgets/GraphicsItem.hpp>
#include <qnamespace.h>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include "SlotHandle.hpp"

namespace Scenario
{
SlotHandle::SlotHandle(const IntervalPresenter& slotView, int slotIndex, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{slotView}
    , m_width{slotView.view()->boundingRect().width()}
    , m_slotIndex{slotIndex}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setCursor(Qt::SizeVerCursor);
}

int SlotHandle::slotIndex() const
{
  return m_slotIndex;
}

void SlotHandle::setSlotIndex(int v)
{
  m_slotIndex = v;
}

QRectF SlotHandle::boundingRect() const
{
  return {0., 0., m_width - 2., handleHeight()};
}

void SlotHandle::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& style = ScenarioStyle::instance();

  painter->fillRect(boundingRect(), style.ProcessViewBorder.getBrush());
}

void SlotHandle::setWidth(qreal width)
{
  prepareGeometryChange();
  m_width = width;
  update();
}

void SlotHandle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.pressed(event->scenePos());
  event->accept();
}

void SlotHandle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  static bool moving = false;
  if(!moving)
  {
    moving = true;
    auto p = event->scenePos();
    m_presenter.moved(p);

    auto view = getView(*this);
    if(view)
        view->ensureVisible(p.x(), p.y(), 1, 1);
    moving = false;
  }
  event->accept();
}

void SlotHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.released(event->scenePos());
  event->accept();
}



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

void SlotHeader::setMini(bool b)
{
  m_mini = b;
  update();
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
  if(!m_mini) {
    painter->setPen(style.IntervalHeaderSeparator);
    painter->setBrush(style.NoBrush);

    // Grip
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
    const double centerX = m_width - 8.;
    const double centerY = 7.5;
    double r = 4.5;
    painter->setBrush(style.MinimapBrush);
    painter->drawEllipse(QPointF{centerX, centerY}, r, r);
    r -= 1.;
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(style.TimeRulerSmallPen);
    painter->drawLine(QPointF{centerX, centerY - r}, QPointF{centerX, centerY + r});
    painter->drawLine(QPointF{centerX - r, centerY }, QPointF{centerX + r, centerY });
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
  if(xpos >= 0 && xpos < 16)
  {
  }
  else if(xpos >= m_width - 16)
  {
    // menu
    m_presenter.requestSlotMenu(m_slotIndex, event->screenPos(), event->scenePos());
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
