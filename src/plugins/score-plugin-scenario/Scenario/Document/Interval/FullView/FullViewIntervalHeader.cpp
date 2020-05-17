// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "FullViewIntervalHeader.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/WidgetWrapper.hpp>

#include <QGraphicsView>
#include <QPainter>
#include <QPoint>

#include <cmath>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
static constexpr qreal fullViewHeaderBarX = 5.;
static constexpr qreal fullViewHeaderBarY = 2.;
FullViewIntervalHeader::FullViewIntervalHeader(
    const score::DocumentContext& ctx,
    QGraphicsItem* parent)
    : IntervalHeader{parent}, m_bar{ctx, this}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
  m_bar.setPos(fullViewHeaderBarX, fullViewHeaderBarY);

  con(m_bar, &AddressBarItem::needRedraw, this, [&]() { update(); });
}

AddressBarItem& FullViewIntervalHeader::bar()
{
  return m_bar;
}

QRectF FullViewIntervalHeader::boundingRect() const
{
  return {0., 0., m_width, IntervalHeader::headerHeight()};
}

void FullViewIntervalHeader::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  // TODO find a better way to update this ?
  painter->setRenderHint(QPainter::Antialiasing, false);

  double textWidth = m_bar.width();

  // If the centered text is hidden, we put it at the left so that it's on the
  // view.
  // We have to compute the visible part of the header
  auto view = getView(*this);
  if (!view)
    return;

  // Note: if the interval always has its pos() in (0; 0), we can
  // safely remove the call to mapToScene.

  const double text_left = view->mapFromScene(mapToScene(QPointF{fullViewHeaderBarX, 0.})).x();
  const double text_right = text_left + textWidth;
  double x = fullViewHeaderBarX;
  const constexpr double min_x = fullViewHeaderBarX;
  const double max_x = view->width() - 30.;

  if (text_left <= min_x)
  {
    // Compute the pixels needed to add to have top-left at 0
    x = x - text_left + min_x;
  }
  else if (text_right >= max_x)
  {
    // Compute the pixels needed to add to have top-right at max
    x = x - text_right + max_x;
  }
  x = std::max(x, 5.);

  if (std::abs(m_bar.pos().x() - x) > 0.1)
  {
    m_bar.setPos(x, fullViewHeaderBarY);
  }
}
}
