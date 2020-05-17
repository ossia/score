#pragma once
#include <Scenario/Document/Interval/IntervalView.hpp>

#include <QRect>

#include <verdigris>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class FullViewIntervalPresenter;
class FullViewIntervalView final : public IntervalView
{
  W_OBJECT(FullViewIntervalView)

public:
  FullViewIntervalView(FullViewIntervalPresenter& presenter, QGraphicsItem* parent);

  ~FullViewIntervalView() override;

  void drawPaths(
      QPainter& p,
      QRectF visibleRect,
      const score::Brush& defaultColor,
      const Process::Style& skin);
  void updatePaths() final override;
  void updatePlayPaths() final override;
  void updateOverlayPos();

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void setGuiWidth(double);

  void setSelected(bool selected);

private:
  void drawDashedPath(QPainter& p, QRectF visibleRect, const Process::Style& skin);
  void drawPlayDashedPath(QPainter& p, QRectF visibleRect, const Process::Style& skin);
  double m_guiWidth{};
};
}
