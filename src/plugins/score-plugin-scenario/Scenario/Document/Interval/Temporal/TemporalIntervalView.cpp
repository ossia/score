// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "TemporalIntervalView.hpp"

#include "TemporalIntervalPresenter.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/IntervalMenuOverlay.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>

#include <score/model/Skin.hpp>

#include <QBrush>
#include <QCursor>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TemporalIntervalView)
class QGraphicsSceneHoverEvent;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TemporalIntervalView::TemporalIntervalView(
    TemporalIntervalPresenter& presenter,
    QGraphicsItem* parent)
    : IntervalView{presenter, parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);

  this->setZValue(ZPos::Interval);
}

TemporalIntervalView::~TemporalIntervalView() {}

QRectF TemporalIntervalView::boundingRect() const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  return {x, -1. , rectW, qreal(intervalAndRackHeight()) + 1.};
}

const TemporalIntervalPresenter& TemporalIntervalView::presenter() const
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
  const qreal play_w = m_waiting ? playWidth() : 0.;

  // Paths
  if (play_w <= 0.)
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
        // if(play_w < min_w)
        {
          solidPath.lineTo(min_w, 0.);
        }
      }

      if (play_w > min_w)
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
      // if(play_w < def_w)
      {
        solidPath.lineTo(def_w, 0.);
      }
    }
    else
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0.);
        // if(play_w < min_w)
        {
          solidPath.lineTo(min_w, 0.);
        }
      }

      if (play_w > min_w)
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
  if (play_w <= 0.)
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

      if (play_w > min_w)
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

      if (play_w > min_w)
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
    QPainter* p,
    const QStyleOptionGraphicsItem*,
    QWidget*)
{
  auto& painter = *p;

  painter.setRenderHint(QPainter::Antialiasing, false);
  auto& skin = Process::Style::instance();


  const qreal def_w = defaultWidth();

  // Draw the stuff present if there is a rack *in the model* ?

  auto& c = presenter().model();
  if (c.smallViewVisible())
  {
    // Background
    auto rect = boundingRect();
    rect.adjust(0, 0, 0, SlotHandle::handleHeight() - 4 );
    rect.setWidth(def_w);

    painter.fillRect(rect, m_presenter.model().metadata().getColor().getBrush());
  }

  // Colors
  const auto& defaultColor = this->intervalColor(skin);

  // Drawing
  if (!solidPath.isEmpty())
  {
    skin.IntervalSolidPen.setBrush(defaultColor);
    painter.setPen(skin.IntervalSolidPen);
    painter.drawPath(solidPath);
  }

  if (!dashedPath.isEmpty())
  {
    skin.IntervalDashPen.setBrush(defaultColor);
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
  else
  {
    // qDebug() << " no solid played path" << playedSolidPath.boundingRect();
  }

  if (!waitingDashedPath.isEmpty())
  {
    if (this->m_waiting)
    {
      skin.IntervalWaitingDashPen.setBrush(
          skin.IntervalWaitingDashFill.getBrush());
      painter.setPen(skin.IntervalWaitingDashPen);
      painter.drawPath(waitingDashedPath);
    }
  }

  if (!playedDashedPath.isEmpty())
  {
    if (this->m_waiting)
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


  /*
  painter.setPen(Qt::red);
  painter.setBrush(Qt::green);
  //painter.drawRect(boundingRect());
  painter.drawPath(shape());
  */

#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter.setPen(Qt::darkRed);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(boundingRect());
#endif
}

void TemporalIntervalView::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  if (h->pos().y() < 4)
    setUngripCursor();
  else
    unsetCursor();

  intervalHoverEnter();
}

void TemporalIntervalView::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  unsetCursor();
  intervalHoverLeave();
}

void TemporalIntervalView::setSelected(bool selected)
{
  m_selected = selected;
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

}
