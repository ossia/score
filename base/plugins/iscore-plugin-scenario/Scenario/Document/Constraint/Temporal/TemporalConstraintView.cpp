
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QCursor>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <qnamespace.h>
#include <Scenario/Document/Constraint/SlotHandle.hpp>

#include <Scenario/Document/Constraint/ConstraintMenuOverlay.hpp>
#include "TemporalConstraintPresenter.hpp"
#include "TemporalConstraintView.hpp"
#include <Scenario/Document/Constraint/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ConstraintView.hpp>
#include <iscore/model/Skin.hpp>

class QGraphicsSceneHoverEvent;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TemporalConstraintView::TemporalConstraintView(
    TemporalConstraintPresenter& presenter, QGraphicsItem* parent)
  : ConstraintView{presenter, parent}
  , m_bgColor{ScenarioStyle::instance().ConstraintDefaultBackground}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);
  this->setAcceptDrops(true);

  this->setZValue(ZPos::Constraint);
}

QRectF TemporalConstraintView::boundingRect() const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  return {x, -4, rectW, qreal(constraintAndRackHeight())};
}

const TemporalConstraintPresenter&TemporalConstraintView::presenter() const
{
  return static_cast<const TemporalConstraintPresenter&>(m_presenter);
}

void TemporalConstraintView::updatePaths()
{
  solidPath = QPainterPath{};
  dashedPath = QPainterPath{};
  playedSolidPath = QPainterPath{};
  playedDashedPath = QPainterPath{};
  waitingDashedPath = QPainterPath{};

  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();

  // Paths
  if(play_w <= 0)
  {
    if (infinite())
    {
      if (min_w != 0.)
      {
        solidPath.lineTo(min_w, 0);
      }

      // TODO end state should be hidden
      dashedPath.moveTo(min_w, 0);
      dashedPath.lineTo(def_w, 0);
    }
    else if (min_w == max_w) // TODO rigid()
    {
      solidPath.lineTo(def_w, 0);
    }
    else
    {
      if (min_w != 0.)
      {
        solidPath.lineTo(min_w, 0);
      }
      dashedPath.moveTo(min_w, 0);
      dashedPath.lineTo(max_w, 0);
    }
  }
  else
  {
    if (infinite())
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0);
        if(play_w < min_w)
        {
          solidPath.moveTo(play_w, 0);
          solidPath.lineTo(min_w, 0);
        }
      }

      if(play_w > min_w)
      {
        playedDashedPath.moveTo(min_w, 0);
        playedDashedPath.lineTo(std::min(def_w, play_w), 0);

        waitingDashedPath.moveTo(min_w, 0);
        waitingDashedPath.lineTo(def_w, 0);
      }
      else
      {
        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(def_w, 0);
      }
    }
    else if (min_w == max_w) // TODO rigid()
    {
      playedSolidPath.lineTo(std::min(play_w, def_w), 0);
      if(play_w < def_w)
      {
        solidPath.moveTo(play_w, 0);
        solidPath.lineTo(def_w, 0);
      }
    }
    else
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0);
        if(play_w < min_w)
        {
          solidPath.moveTo(play_w, 0);
          solidPath.lineTo(min_w, 0);
        }
      }

      if(play_w > min_w)
      {
        playedDashedPath.moveTo(min_w, 0);
        playedDashedPath.lineTo(play_w, 0);

        waitingDashedPath.moveTo(min_w, 0);
        waitingDashedPath.lineTo(max_w, 0);
      }
      else
      {
        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(max_w, 0);
      }
    }
  }
  update();
}

void TemporalConstraintView::paint(
    QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
  auto& painter = *p;
  painter.setRenderHint(QPainter::Antialiasing, false);
  auto& skin = ScenarioStyle::instance();

  const qreal def_w = defaultWidth();

  // Draw the stuff present if there is a rack *in the model* ?

  if (presenter().model().smallViewVisible())
  {
    // Background
    auto rect = boundingRect();
    rect.adjust(0, 4, 0, SlotHandle::handleHeight());
    rect.setWidth(def_w);

    auto bgColor = m_bgColor.getColor().color();
    bgColor.setAlpha(m_hasFocus ? 84 : 76);
    painter.fillRect(rect, bgColor);

    // Fake timenode continuation
    skin.ConstraintRackPen.setBrush(skin.RackSideBorder.getColor());
    painter.setPen(skin.ConstraintRackPen);
    painter.drawLine(rect.topLeft(), rect.bottomLeft());
    painter.drawLine(rect.topRight(), rect.bottomRight());
  }

  // Colors
  auto defaultColor = this->constraintColor(skin);

  skin.ConstraintSolidPen.setBrush(defaultColor);
  skin.ConstraintDashPen.setBrush(defaultColor);

  // Drawing
  if (!solidPath.isEmpty())
  {
    painter.setPen(skin.ConstraintSolidPen);
    painter.drawPath(solidPath);
  }

  if (!dashedPath.isEmpty())
  {
    painter.setPen(skin.ConstraintDashPen);
    painter.drawPath(dashedPath);
    skin.ConstraintDashPen.setDashOffset(0);
  }

  if (!playedSolidPath.isEmpty())
  {
    skin.ConstraintPlayPen.setBrush(skin.ConstraintPlayFill.getColor());

    painter.setPen(skin.ConstraintPlayPen);
    painter.drawPath(playedSolidPath);
  }

  if(!waitingDashedPath.isEmpty())
  {
    if(this->m_waiting)
    {
      skin.ConstraintWaitingDashPen.setBrush(skin.ConstraintWaitingDashFill.getColor());
      painter.setPen(skin.ConstraintWaitingDashPen);
      painter.drawPath(waitingDashedPath);
    }
  }

  if (!playedDashedPath.isEmpty())
  {
    if(this->m_waiting)
    {
      skin.ConstraintPlayDashPen.setBrush(skin.ConstraintPlayDashFill.getColor());
    }
    else
    {
      skin.ConstraintPlayDashPen.setBrush(skin.ConstraintPlayFill.getColor());
    }

    painter.setPen(skin.ConstraintPlayDashPen);
    painter.drawPath(playedDashedPath);
  }

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  painter.setPen(Qt::darkRed);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(boundingRect());
#endif
}

void TemporalConstraintView::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  if(h->pos().y() < 4)
    setUngripCursor();
  else
    unsetCursor();

  updateOverlay();
  emit constraintHoverEnter();
}

void TemporalConstraintView::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  unsetCursor();
  updateOverlay();
  emit constraintHoverLeave();
}

void TemporalConstraintView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragEnterEvent(event);
  updateOverlay();
  event->accept();
}

void TemporalConstraintView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragLeaveEvent(event);
  updateOverlay();
  event->accept();
}

void TemporalConstraintView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  emit dropReceived(event->pos(), event->mimeData());

  event->accept();
}

void TemporalConstraintView::setLabel(const QString& label)
{
  m_labelItem.setText(label);
  updateLabelPos();
}

void TemporalConstraintView::enableOverlay(bool b)
{
  if(b)
  {
    m_overlay = new ConstraintMenuOverlay{this};
    updateOverlayPos();
  }
  else
  {
    delete m_overlay;
    m_overlay = nullptr;
  }
}

void TemporalConstraintView::setExecutionDuration(const TimeVal& progress)
{
  // FIXME this should be merged with the slot in ConstraintPresenter!!!
  // Also make a setting to disable it since it may take a lot of time
  if (!qFuzzyCompare(progress.msec(), 0))
  {
    if (!m_counterItem.isVisible())
      m_counterItem.setVisible(true);
    updateCounterPos();

    m_counterItem.setText(progress.toString());
  }
  else
  {
    m_counterItem.setVisible(false);
  }
  update();
}

void TemporalConstraintView::setLabelColor(iscore::ColorRef labelColor)
{
  m_labelItem.setColor(labelColor);
  update();
}

}
