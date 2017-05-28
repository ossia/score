#pragma once
#include <QRect>
#include <Scenario/Document/Constraint/ConstraintHeader.hpp>
#include <Scenario/Document/Constraint/FullView/AddressBarItem.hpp>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class AddressBarItem;
class FullViewConstraintHeader final : public ConstraintHeader
{
public:
  FullViewConstraintHeader(QGraphicsItem*);

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
