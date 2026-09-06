#pragma once
#include <Scenario/Document/Interval/IntervalHeader.hpp>

#include <QRect>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace score
{
struct DocumentContext;
}
namespace Scenario
{
/**
 * Header band above the full view interval. Empty: the interval's path is
 * shown in the document's navigation bar (AddressBarWidget).
 */
class FullViewIntervalHeader final : public IntervalHeader
{
public:
  FullViewIntervalHeader(const score::DocumentContext& ctx, QGraphicsItem*);

  void setState(State s) override { }

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
