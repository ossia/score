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
#include <Scenario/Document/Event/EventModel.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsItem>
#include <QGraphicsView>
#include <QPainter>
#include <QPainterPath>
#include <qnamespace.h>

#include <QStyleOption>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TemporalIntervalView)
class QGraphicsSceneHoverEvent;
class QStyleOptionGraphicsItem;
class QWidget;
struct IntervalPixmaps
{
  void update(const Process::Style& style)
  {
    auto& dashPen = style.IntervalDashPen(style.IntervalBase());
    auto& dashSelectedPen = style.IntervalDashPen(style.IntervalSelected());
    if(oldBase == dashPen.color() && oldSelected == dashSelectedPen.color())
      return;

    static constexpr double dash_width = 18.;
    {
      const auto pen_width = dashPen.widthF();

      QImage image(dash_width, pen_width, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter p; p.begin(&image);
      p.setPen(dashPen);
      p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
      p.end();

      dashed = QPixmap::fromImage(image);
    }

    {
      const auto pen_width = dashSelectedPen.widthF();

      QImage image(dash_width, pen_width, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter p; p.begin(&image);
      p.setPen(dashSelectedPen);
      p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
      p.end();

      dashedSelected = QPixmap::fromImage(image);
    }

    {
      auto dashPlayPen = style.IntervalDashPen(style.IntervalPlayDashFill());
      QColor pulse_base = style.skin.Pulse1.color();
      for(int i = 0; i < 25; i++)
      {
        float alpha = 0.5 + 0.02 * i;
        pulse_base.setAlphaF(alpha);
        dashPlayPen.setColor(pulse_base);

        const auto pen_width = dashSelectedPen.widthF();

        QImage image(dash_width, pen_width, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter p; p.begin(&image);
        p.setPen(dashPlayPen);
        p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
        p.end();

        playDashed[i] = QPixmap::fromImage(image);
      }
    }
    oldBase = dashPen.color();
    oldSelected = dashSelectedPen.color();
  }

  QColor oldBase, oldSelected;
  QPixmap dashed;
  QPixmap dashedSelected;
  std::array<QPixmap, 25> playDashed;
};

static IntervalPixmaps& intervalPixmaps(const Process::Style& style)
{
  static IntervalPixmaps pixmaps;
  pixmaps.update(style);
  return pixmaps;
}

namespace Scenario
{
TemporalIntervalView::TemporalIntervalView(
    TemporalIntervalPresenter& presenter,
    QGraphicsItem* parent)
    : IntervalView{presenter, parent}
{
  this->setFlag(ItemClipsChildrenToShape, true);
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
  playedSolidPath = QPainterPath{};

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

static void draw_dashes(qreal from, qreal to, QPainter& p, const QRectF& visibleRect, const QPixmap& pixmap) {
  from = std::max(from, visibleRect.left());
  to = std::min(to, visibleRect.right());
  const qreal w = pixmap.width();
  const qreal h = - 2.;
  for(; from < to - w; from+=w) {
    p.drawPixmap(from, h, pixmap);
  }

  p.drawPixmap(QRectF{from, h, -1, -1}, pixmap, QRectF{0, 0, to - from, h});
}

void TemporalIntervalView::drawDashedPath(
    QPainter& p,
    QRectF visibleRect,
    const Process::Style& skin)
{
  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();

  auto& pixmaps = intervalPixmaps(skin);
  auto& dash_pixmap = !this->m_selected ? pixmaps.dashed : pixmaps.dashedSelected;

  // Paths
  if(play_w <= min_w)
  {
    if (infinite())
    {
      draw_dashes(min_w, def_w, p, visibleRect, dash_pixmap);
    }
    else if (min_w != max_w)
    {
      draw_dashes(min_w, max_w, p, visibleRect, dash_pixmap);
    }
  }
}

void TemporalIntervalView::drawPlayDashedPath(
    QPainter& p,
    QRectF visibleRect,
    const Process::Style& skin)
{
  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();


  // Paths
  if (play_w <= min_w)
    return;
  if(presenter().model().duration.isRigid())
    return;

  double actual_min = std::max(min_w, visibleRect.left());
  double actual_max = std::min(infinite() ? def_w : max_w, visibleRect.right());

  auto& pixmaps = intervalPixmaps(skin);

  // waiting
  const int idx = m_waiting ? skin.skin.PulseIndex : 0;
  draw_dashes(actual_min, actual_max, p, visibleRect, pixmaps.playDashed[idx]);

  // played
  draw_dashes(actual_min, std::min(actual_max, play_w), p, visibleRect, pixmaps.playDashed.back());

  p.setPen(skin.IntervalPlayLinePen(skin.IntervalPlayFill()));

  p.drawLine(QPointF{actual_min, -0.5}, QPointF{std::min(actual_max, play_w), -0.5});
}

void TemporalIntervalView::updatePlayPaths()
{
  playedSolidPath = QPainterPath{};

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

void TemporalIntervalView::paint(
    QPainter* p,
    const QStyleOptionGraphicsItem* so,
    QWidget*)
{
  auto view = ::getView(*this);
  if(!view)
    return;
  auto& painter = *p;
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

  painter.setRenderHint(QPainter::Antialiasing, false);
  auto& skin = Process::Style::instance();

  QRectF visibleRect = QRectF{itemDrawableTopLeft, itemDrawableBottomRight};
  auto& c = presenter().model();
  if (c.smallViewVisible())
  {
    // Background
    visibleRect.adjust(0.5, 2., -0.5, -2.);

    painter.fillRect(visibleRect,
                     m_presenter.model().metadata().getColor().getBrush());
  }

  // Colors
  const auto& defaultColor = this->intervalColor(skin);

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
