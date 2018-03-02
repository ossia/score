// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QList>
#include <QPoint>
#include <cmath>
#include <Process/Style/ScenarioStyle.hpp>
#include "FullViewIntervalHeader.hpp"
#include <score/widgets/GraphicsItem.hpp>
#include <score/widgets/WidgetWrapper.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
FullViewIntervalHeader::FullViewIntervalHeader(
    const score::DocumentContext& ctx,
    QGraphicsItem* parent)
    : IntervalHeader{parent}, m_bar{ctx, this}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
  m_bar.setPos(10., 10.);

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
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);

  const auto& skin = ScenarioStyle::instance();
  painter->setPen(skin.IntervalHeaderSeparator);
  painter->drawLine(
              QPointF{0., (double)IntervalHeaderHeight},
              QPointF{m_width, (double)IntervalHeaderHeight});

  double textWidth = m_bar.width();

  // If the centered text is hidden, we put it at the left so that it's on the
  // view.
  // We have to compute the visible part of the header
  auto view = getView(*this);
  if(!view)
      return;

  // Note: if the interval always has its pos() in (0; 0), we can
  // safely remove the call to mapToScene.

  const double text_left
      = view->mapFromScene(mapToScene(QPointF{m_width / 2. - textWidth / 2., 0.})).x();
  const double text_right = text_left + textWidth;
  double x = (m_width - textWidth) / 2.;
  const constexpr double min_x = 10.;
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
    m_bar.setPos(x, 8.);
}
}
