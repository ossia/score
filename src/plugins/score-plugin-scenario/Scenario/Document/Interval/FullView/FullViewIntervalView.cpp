// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "FullViewIntervalView.hpp"

#include "FullViewIntervalPresenter.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <Scenario/Document/Interval/IntervalPixmaps.hpp>
#include <score/graphics/PainterPath.hpp>

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QPainter>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::FullViewIntervalView)

namespace Scenario
{
FullViewIntervalView::FullViewIntervalView(
    FullViewIntervalPresenter& presenter,
    QGraphicsItem* parent)
    : IntervalView{presenter, parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);
  this->setFlag(ItemIsSelectable);

  this->setZValue(ZPos::Interval);
}

FullViewIntervalView::~FullViewIntervalView() {}


void FullViewIntervalView::drawDashedPath(
    QPainter& p,
    QRectF visibleRect,
    const Process::Style& skin)
{
  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal gui_w = m_guiWidth;
  const qreal play_w = playWidth();

  auto& pixmaps = intervalPixmaps(skin);
  auto& dash_pixmap = !this->m_selected ? pixmaps.dashed : pixmaps.dashedSelected;

  // Paths
  if(play_w <= min_w)
  {
    if (infinite())
    {
      IntervalPixmaps::drawDashes(min_w, gui_w, p, visibleRect, dash_pixmap);
    }
    else if (min_w != max_w)
    {
      IntervalPixmaps::drawDashes(min_w, max_w, p, visibleRect, dash_pixmap);
    }
  }
}

void FullViewIntervalView::drawPlayDashedPath(
    QPainter& p,
    QRectF visibleRect,
    const Process::Style& skin)
{
  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal gui_w = m_guiWidth;
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();


  // Paths
  if (play_w <= min_w)
    return;
  if(presenter().model().duration.isRigid())
    return;

  double actual_min = std::max(min_w, visibleRect.left());
  double actual_max = std::min(infinite() ? gui_w : max_w, visibleRect.right());

  auto& pixmaps = intervalPixmaps(skin);

  // waiting
  const int idx = m_waiting ? skin.skin.PulseIndex : 0;
  IntervalPixmaps::drawDashes(actual_min, actual_max, p, visibleRect, pixmaps.playDashed[idx]);

  // played
  IntervalPixmaps::drawDashes(actual_min, std::min(actual_max, play_w), p, visibleRect, pixmaps.playDashed.back());

  p.setPen(skin.IntervalPlayLinePen(skin.IntervalPlayFill()));

  p.drawLine(QPointF{actual_min, -0.5}, QPointF{std::min(actual_max, play_w), -0.5});
}

void FullViewIntervalView::updatePaths() {

  clearPainterPath(solidPath);
  clearPainterPath(playedSolidPath);

  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();

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
      // - dashedPath.moveTo(min_w, 0.);
      // - dashedPath.lineTo(def_w, 0.);
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
    }
    else if (min_w == max_w) // TODO rigid()
    {
      playedSolidPath.lineTo(std::min(play_w, def_w), 0.);
      solidPath.lineTo(def_w, 0.);
    }
    else
    {
      if (min_w != 0.)
      {
        playedSolidPath.lineTo(std::min(play_w, min_w), 0.);
        solidPath.lineTo(min_w, 0.);
      }
    }
  }

}
void FullViewIntervalView::drawPaths(
      QPainter& p,
      QRectF visibleRect,
      const score::Brush& defaultColor,
      const Process::Style& skin)
{
}

void FullViewIntervalView::updatePlayPaths()
{
  clearPainterPath(playedSolidPath);

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
    }
  }
}

void FullViewIntervalView::updateOverlayPos() {}

void FullViewIntervalView::setSelected(bool selected)
{
  m_selected = selected;
  setZValue(m_selected ? ZPos::SelectedInterval : ZPos::Interval);
  update();
}
QRectF FullViewIntervalView::boundingRect() const
{
  return {0,
          -3,
          qreal(std::max(defaultWidth(), m_guiWidth)) + 3,
          qreal(intervalAndRackHeight()) + 6};
}

void FullViewIntervalView::paint(
    QPainter* p,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto view = ::getView(*this);
  if(!view)
    return;

  QPointF sceneDrawableTopLeft = view->mapToScene(-10, 0);
  QPointF sceneDrawableBottomRight = view->mapToScene(view->width() + 10, view->height() + 10);
  QPointF itemDrawableTopLeft = this->mapFromScene(sceneDrawableTopLeft);
  QPointF itemDrawableBottomRight = this->mapFromScene(sceneDrawableBottomRight);

  itemDrawableTopLeft.rx() = std::max(itemDrawableTopLeft.x(), 0.);
  itemDrawableTopLeft.ry() = std::max(itemDrawableTopLeft.y(), 0.);

  itemDrawableBottomRight.rx() = std::min(itemDrawableBottomRight.x(), boundingRect().width());
  itemDrawableBottomRight.ry() = std::min(itemDrawableBottomRight.y(), boundingRect().height());
  if(itemDrawableTopLeft.x() > boundingRect().width())
  {
    return;
  }
  if(itemDrawableBottomRight.y() > boundingRect().height())
  {
    return;
  }

  if(itemDrawableTopLeft.y() > 20)
  {
    return;
  }


  auto& painter = *p;
  auto& skin = Process::Style::instance();

  painter.setBrush(skin.NoBrush());
  painter.setRenderHint(QPainter::Antialiasing, false);

  const auto& defaultColor = this->intervalColor(skin);

  const auto visibleRect = QRectF{itemDrawableTopLeft, itemDrawableBottomRight};
  //drawPaths(painter, visibleRect, defaultColor, skin);

  // Drawing
  if (!solidPath.isEmpty())
  {
    painter.setPen(skin.IntervalSolidPen(defaultColor));
    painter.drawPath(solidPath);
  }

  drawDashedPath(painter, visibleRect, skin);

  if (!playedSolidPath.isEmpty())
  {
    painter.setPen(skin.IntervalSolidPen(skin.IntervalPlayFill()));
    painter.drawPath(playedSolidPath);
  }

  drawPlayDashedPath(painter, visibleRect, skin);
#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter.setPen(Qt::red);
  painter.drawRect(boundingRect());
#endif
}

void FullViewIntervalView::setGuiWidth(double w)
{
  m_guiWidth = w;
  update();
}
}
