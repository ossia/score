#pragma once
#include <QRect>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/FullView/AddressBarItem.hpp>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class AddressBarItem;
class FullViewIntervalHeader final : public IntervalHeader
{
  public:
    FullViewIntervalHeader(
        const score::DocumentContext& ctx,
        QGraphicsItem*);

    AddressBarItem& bar();

    void setState(State s) override
    {
    }

    QRectF boundingRect() const override;
    void paint(
        QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

  private:
    AddressBarItem m_bar;
};
}
