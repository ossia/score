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
FullViewIntervalHeader::FullViewIntervalHeader(
    const score::DocumentContext& ctx, QGraphicsItem* parent)
    : IntervalHeader{parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
  this->setFlag(QGraphicsItem::ItemHasNoContents, true);
}

QRectF FullViewIntervalHeader::boundingRect() const
{
  return {0., 0., m_width, IntervalHeader::headerHeight()};
}

void FullViewIntervalHeader::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}
}
