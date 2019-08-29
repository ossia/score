// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "FullViewIntervalView.hpp"

#include "FullViewIntervalPresenter.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <score/graphics/GraphicsItem.hpp>

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

void FullViewIntervalView::updatePaths() { }
void FullViewIntervalView::drawPaths(
      QPainter& p,
      QRectF visibleRect,
      const score::Brush& defaultColor,
      const Process::Style& skin)
{
  solidPath = QPainterPath{};
  playedSolidPath = QPainterPath{};

  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal gui_w = m_guiWidth;
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();
  const auto& solidPen = skin.IntervalSolidPen(defaultColor);
  const auto& dashPen = skin.IntervalDashPen(defaultColor);

  const auto& solidPlayPen = skin.IntervalSolidPen(skin.IntervalPlayFill());
  const auto& dashPlayPen = skin.IntervalDashPen(skin.IntervalPlayFill());
  const auto& waitingPlayPen = skin.IntervalDashPen(skin.IntervalWaitingDashFill());

  // Paths
  if (play_w <= 0.)
  {
    if (infinite())
    {
      if (min_w != 0.)
      {
        p.setPen(solidPen);
        p.drawLine(QPointF{0, 0}, QPointF{min_w, 0.});
      }

      // TODO end state should be hidden
      {
        p.setPen(dashPen);
        p.drawLine(QPointF{min_w, 0}, QPointF{gui_w, 0.});
      }
    }
    else if (min_w == max_w) // TODO rigid()
    {
      p.setPen(solidPen);
      p.drawLine(QPointF{0, 0}, QPointF{def_w, 0.});
    }
    else
    {
      if (min_w != 0.)
      {
        p.setPen(solidPen);
        p.drawLine(QPointF{0, 0}, QPointF{min_w, 0.});
      }
      p.setPen(dashPen);
      p.drawLine(QPointF{min_w, 0}, QPointF{max_w, 0.});
    }
  }
  else
  {
    if (infinite())
    {
      if (min_w != 0.)
      {
        const auto min_pt = std::min(play_w, min_w);
        p.setPen(solidPlayPen);
        p.drawLine(QPointF{0, 0}, QPointF{min_pt, 0.});

        if(min_pt < min_w)
        {
          p.setPen(solidPen);
          p.drawLine(QPointF{min_pt, 0}, QPointF{min_w, 0.});
        }
      }

      if (play_w > min_w)
      {
        const auto min_pt = std::min(def_w, play_w);

        if(min_pt < def_w)
        {
          p.setPen(waitingPlayPen);
          p.drawLine(QPointF{min_w, 0.}, QPointF{def_w, 0.});
        }

        p.setPen(dashPlayPen);
        p.drawLine(QPointF{min_w, 0.}, QPointF{min_pt, 0.});
      }
      else
      {
        p.setPen(dashPen);
        p.drawLine(QPointF{min_w, 0}, QPointF{def_w, 0.});
      }
    }
    else if (min_w == max_w) // TODO rigid()
    {
      const auto min_pt = std::min(play_w, def_w);
      p.setPen(solidPlayPen);
      p.drawLine(QPointF{0, 0}, QPointF{min_pt, 0.});

      if(min_pt < def_w)
      {
        p.setPen(solidPen);
        p.drawLine(QPointF{min_pt, 0}, QPointF{def_w, 0.});
      }
    }
    else
    {
      if (min_w != 0.)
      {
        const auto min_pt = std::min(play_w, min_w);
        p.setPen(solidPlayPen);
        p.drawLine(QPointF{0, 0}, QPointF{min_pt, 0.});
        if(min_pt < min_w)
        {
          p.setPen(solidPen);
          p.drawLine(QPointF{min_pt, 0}, QPointF{min_w, 0.});
          solidPath.lineTo(min_w, 0.);
        }
      }

      if (play_w > min_w)
      {
        if(max_w > play_w)
        {
          p.setPen(waitingPlayPen);
          p.drawLine(QPointF{min_w, 0.}, QPointF{max_w, 0.});
        }

        p.setPen(dashPlayPen);
        p.drawLine(QPointF{min_w, 0.}, QPointF{play_w, 0.});
      }
      else
      {
        p.setPen(dashPen);
        p.drawLine(QPointF{min_w, 0.}, QPointF{max_w, 0.});
      }
    }
  }
}

void FullViewIntervalView::updatePlayPaths()
{
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
  drawPaths(painter, visibleRect, defaultColor, skin);

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
