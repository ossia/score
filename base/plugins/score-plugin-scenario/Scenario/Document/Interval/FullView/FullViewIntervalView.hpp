#pragma once
#include <QRect>
#include <Scenario/Document/Interval/IntervalView.hpp>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class FullViewIntervalPresenter;
class FullViewIntervalView final : public IntervalView
{
  Q_OBJECT

public:
  FullViewIntervalView(
      FullViewIntervalPresenter& presenter, QGraphicsItem* parent);

  ~FullViewIntervalView() override;

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
