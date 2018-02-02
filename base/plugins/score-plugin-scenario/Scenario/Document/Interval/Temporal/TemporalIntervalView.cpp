// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QCursor>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <qnamespace.h>
#include <Scenario/Document/Interval/SlotHandle.hpp>

#include <Scenario/Document/Interval/IntervalMenuOverlay.hpp>
#include "TemporalIntervalPresenter.hpp"
#include "TemporalIntervalView.hpp"
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <score/model/Skin.hpp>

class QGraphicsSceneHoverEvent;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TemporalIntervalView::TemporalIntervalView(
    TemporalIntervalPresenter& presenter, QGraphicsItem* parent)
  : IntervalView{presenter, parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);
  this->setAcceptDrops(true);

  this->setZValue(ZPos::Interval);
}

QRectF TemporalIntervalView::boundingRect() const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  return {x, -4, rectW, qreal(intervalAndRackHeight())};
}

const TemporalIntervalPresenter&TemporalIntervalView::presenter() const
{
  return static_cast<const TemporalIntervalPresenter&>(m_presenter);
}

void TemporalIntervalView::updatePaths()
{
  solidPath = QPainterPath{};
  dashedPath = QPainterPath{};
  playedSolidPath = QPainterPath{};
  playedDashedPath = QPainterPath{};
  waitingDashedPath = QPainterPath{};

  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = m_waiting ?  playWidth() : 0.;

  // Paths
  if(play_w <= 0.)
  {
    if (infinite())
    {
      if (min_w != 0.)
      {
        solidPath.lineTo(min_w, 0.);
      }

      // TODO end state should be hidden
      dashedPath.moveTo(min_w, 0.);
      dashedPath.lineTo(def_w, 0.);
    }
    else if (min_w == max_w) // TODO rigid()
    {
      solidPath.lineTo(def_w, 0.);
    }
    else
    {
      if (min_w != 0.)
      {
        solidPath.lineTo(min_w, 0.);
      }
      dashedPath.moveTo(min_w, 0.);
      dashedPath.lineTo(max_w, 0.);
    }
  }
  else
  {
    if (infinite())
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0.);
        //if(play_w < min_w)
        {
          solidPath.lineTo(min_w, 0.);
        }
      }

      if(play_w > min_w)
      {
        playedDashedPath.moveTo(min_w, 0.);
        playedDashedPath.lineTo(std::min(def_w, play_w), 0.);

        waitingDashedPath.moveTo(min_w, 0.);
        waitingDashedPath.lineTo(def_w, 0.);
      }
      else
      {
        dashedPath.moveTo(min_w, 0.);
        dashedPath.lineTo(def_w, 0.);
      }
    }
    else if (min_w == max_w) // TODO rigid()
    {
      playedSolidPath.lineTo(std::min(play_w, def_w), 0.);
      //if(play_w < def_w)
      {
        solidPath.lineTo(def_w, 0.);
      }
    }
    else
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0.);
        //if(play_w < min_w)
        {
          solidPath.lineTo(min_w, 0.);
        }
      }

      if(play_w > min_w)
      {
        playedDashedPath.moveTo(min_w, 0.);
        playedDashedPath.lineTo(play_w, 0.);

        waitingDashedPath.moveTo(min_w, 0.);
        waitingDashedPath.lineTo(max_w, 0.);
      }
      else
      {
        dashedPath.moveTo(min_w, 0.);
        dashedPath.lineTo(max_w, 0.);
      }
    }
  }
}


void TemporalIntervalView::updatePlayPaths()
{
  playedSolidPath = QPainterPath{};
  playedDashedPath = QPainterPath{};
  waitingDashedPath = QPainterPath{};

  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();

  // Paths
  if(play_w <= 0.)
  {
    return;
  }
  else
  {
    if (infinite())
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0.);
      }

      if(play_w > min_w)
      {
        playedDashedPath.moveTo(min_w, 0.);
        playedDashedPath.lineTo(std::min(def_w, play_w), 0.);

        waitingDashedPath.moveTo(min_w, 0.);
        waitingDashedPath.lineTo(def_w, 0.);
      }
    }
    else if (min_w == max_w) // TODO rigid()
    {
      playedSolidPath.lineTo(std::min(play_w, def_w), 0.);
    }
    else
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0.);
      }

      if(play_w > min_w)
      {
        playedDashedPath.moveTo(min_w, 0.);
        playedDashedPath.lineTo(play_w, 0.);

        waitingDashedPath.moveTo(min_w, 0.);
        waitingDashedPath.lineTo(max_w, 0.);
      }
    }
  }
}


void TemporalIntervalView::paint(
    QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
  auto& painter = *p;
  painter.setRenderHint(QPainter::Antialiasing, false);
  auto& skin = ScenarioStyle::instance();

  const qreal def_w = defaultWidth();

  // Draw the stuff present if there is a rack *in the model* ?

  auto& c = presenter().model();
  if (c.smallViewVisible())
  {
    // Background
    auto rect = boundingRect();
    rect.adjust(0, 4, 0, SlotHandle::handleHeight());
    rect.setWidth(def_w);

    auto bgColor = m_presenter.model().metadata().getColor().getBrush().color();
    bgColor.setAlpha(m_hasFocus ? 86 : 70);
    // TODO try to prevent allocation here by storing two copies instead
    painter.fillRect(rect, bgColor);

    // Fake timesync continuation
    skin.IntervalRackPen.setBrush(skin.RackSideBorder.getBrush());
    painter.setPen(skin.IntervalRackPen);
    painter.drawLine(rect.topLeft(), rect.bottomLeft());
    painter.drawLine(rect.topRight(), rect.bottomRight());
  }

  // Colors
  const auto& defaultColor = this->intervalColor(skin);

  skin.IntervalSolidPen.setBrush(defaultColor);
  skin.IntervalDashPen.setBrush(defaultColor);

  // Drawing
  if (!solidPath.isEmpty())
  {
    painter.setPen(skin.IntervalSolidPen);
    painter.drawPath(solidPath);
  }

  if (!dashedPath.isEmpty())
  {
    painter.setPen(skin.IntervalDashPen);
    painter.drawPath(dashedPath);
    skin.IntervalDashPen.setDashOffset(0);
  }

  if (!playedSolidPath.isEmpty())
  {
    skin.IntervalPlayPen.setBrush(skin.IntervalPlayFill.getBrush());

    painter.setPen(skin.IntervalPlayPen);
    painter.drawPath(playedSolidPath);
  }

  if(!waitingDashedPath.isEmpty())
  {
    if(this->m_waiting)
    {
      skin.IntervalWaitingDashPen.setBrush(skin.IntervalWaitingDashFill.getBrush());
      painter.setPen(skin.IntervalWaitingDashPen);
      painter.drawPath(waitingDashedPath);
    }
  }

  if (!playedDashedPath.isEmpty())
  {
    if(this->m_waiting)
    {
      skin.IntervalPlayDashPen.setBrush(skin.IntervalPlayDashFill.getBrush());
    }
    else
    {
      skin.IntervalPlayDashPen.setBrush(skin.IntervalPlayFill.getBrush());
    }

    painter.setPen(skin.IntervalPlayDashPen);
    painter.drawPath(playedDashedPath);
  }

#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter.setPen(Qt::darkRed);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(boundingRect());
#endif
}

void TemporalIntervalView::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  if(h->pos().y() < 4)
    setUngripCursor();
  else
    unsetCursor();

  updateOverlay();
  emit intervalHoverEnter();
}

void TemporalIntervalView::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  unsetCursor();
  updateOverlay();
  emit intervalHoverLeave();
}

void TemporalIntervalView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragEnterEvent(event);
  updateOverlay();
  event->accept();
}

void TemporalIntervalView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragLeaveEvent(event);
  updateOverlay();
  event->accept();
}

void TemporalIntervalView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  emit dropReceived(event->pos(), event->mimeData());

  event->accept();
}

void TemporalIntervalView::setLabel(const QString& label)
{
  m_labelItem.setText(label);
  updateLabelPos();
}

void TemporalIntervalView::enableOverlay(bool b)
{
  if(b)
  {
    m_overlay = new IntervalMenuOverlay{this};
    updateOverlayPos();
  }
  else
  {
    delete m_overlay;
    m_overlay = nullptr;
  }
}

void TemporalIntervalView::setSelected(bool selected)
{
  m_selected = selected;
  setZValue(m_selected ? ZPos::SelectedInterval : ZPos::Interval);
  enableOverlay(selected);
  update();
}

void TemporalIntervalView::setExecutionDuration(const TimeVal& progress)
{
  // FIXME this should be merged with the slot in IntervalPresenter!!!
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

void TemporalIntervalView::updateOverlayPos()
{
  if(m_overlay)
  {
    m_overlay->setPos(defaultWidth() / 2. - m_overlay->boundingRect().width() / 2, -10);
  }
}

void TemporalIntervalView::setLabelColor(score::ColorRef labelColor)
{
  m_labelItem.setColor(labelColor);
  update();
}
}
