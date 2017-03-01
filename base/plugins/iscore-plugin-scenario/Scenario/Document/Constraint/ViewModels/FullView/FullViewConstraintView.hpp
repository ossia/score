#pragma once
#include <QRect>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>

class QQuickPaintedItem;
class QPainter;

class QWidget;

namespace Scenario
{
class FullViewConstraintPresenter;
class FullViewConstraintView final : public ConstraintView
{
  Q_OBJECT

public:
  FullViewConstraintView(
      FullViewConstraintPresenter& presenter, QQuickPaintedItem* parent);

  virtual ~FullViewConstraintView() = default;

  void updatePaths() final override;
  void updateSize() final override;
  void paint(
      QPainter* painter) override;
private:
  QPainterPath solidPath, dashedPath, playedPath;
};
}
