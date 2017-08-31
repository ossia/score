#pragma once
#include <QRect>
#include <Scenario/Document/Constraint/ConstraintView.hpp>

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

  void updatePaths() final override;
  void updatePlayPaths() final override;
  void updateOverlayPos();

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setGuiWidth(double);

  void setSelected(bool selected);
private:
  double m_guiWidth{};
};
}
