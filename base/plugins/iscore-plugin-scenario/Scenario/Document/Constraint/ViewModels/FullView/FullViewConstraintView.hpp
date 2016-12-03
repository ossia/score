#pragma once
#include <QRect>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class FullViewConstraintPresenter;
class FullViewConstraintView final : public ConstraintView
{
  Q_OBJECT

public:
  FullViewConstraintView(
      FullViewConstraintPresenter& presenter, QGraphicsItem* parent);

  virtual ~FullViewConstraintView() = default;

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
};
}
