// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <QCursor>
#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QtGlobal>
#include <qnamespace.h>

#include "FullViewIntervalPresenter.hpp"
#include "FullViewIntervalView.hpp"
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <QGraphicsScene>
#include <QGraphicsView>
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
FullViewIntervalView::FullViewIntervalView(
    FullViewIntervalPresenter& presenter, QGraphicsItem* parent)
    : IntervalView{presenter, parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);
  this->setFlag(ItemIsSelectable);

  this->setZValue(1);
}

void FullViewIntervalView::updatePaths()
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
  const qreal gui_w = m_guiWidth;

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
      dashedPath.lineTo(gui_w, 0.);
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
        dashedPath.lineTo(gui_w, 0.);
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


void FullViewIntervalView::updatePlayPaths()
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

void FullViewIntervalView::updateOverlayPos()
{
}

void FullViewIntervalView::setSelected(bool selected)
{
  m_selected = selected;
  setZValue(m_selected ? ZPos::SelectedInterval : ZPos::Interval);
  update();
}
QRectF FullViewIntervalView::boundingRect() const
{
  return {0, -3, qreal(std::max(defaultWidth(), m_guiWidth)) + 3, qreal(intervalAndRackHeight()) + 6};
}

void FullViewIntervalView::paint(
    QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& painter = *p;
  auto& skin = ScenarioStyle::instance();
  painter.setRenderHint(QPainter::Antialiasing, false);

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
  p.setPen(Qt::red);
  p.drawRect(boundingRect());
#endif
}

void FullViewIntervalView::setGuiWidth(double w)
{
  m_guiWidth = w;
  update();
}
}
