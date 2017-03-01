#pragma once
#include <QRect>
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>

class QQuickPaintedItem;
class QPainter;

class QWidget;

namespace Scenario
{
class AddressBarItem;
class FullViewConstraintHeader final : public ConstraintHeader
{
public:
  FullViewConstraintHeader(QQuickPaintedItem*);

  AddressBarItem* bar() const;

  void setState(State s) override
  {
  }

  void paint(
      QPainter* painter) override;

private:
  AddressBarItem* m_bar{};
};
}
