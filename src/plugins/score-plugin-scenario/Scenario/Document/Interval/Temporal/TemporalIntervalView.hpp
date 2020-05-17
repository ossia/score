#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/ExecutionState.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>

#include <score/graphics/TextItem.hpp>
#include <score/model/ColorReference.hpp>
#include <score/widgets/MimeData.hpp>

#include <QPainter>
#include <QRect>

#include <verdigris>
namespace Process
{
class LayerPresenter;
}
namespace Scenario
{
class TemporalIntervalPresenter;
class IntervalDurations;
class SCORE_PLUGIN_SCENARIO_EXPORT TemporalIntervalView final : public IntervalView
{
  W_OBJECT(TemporalIntervalView)

public:
  TemporalIntervalView(TemporalIntervalPresenter& presenter, QGraphicsItem* parent);
  ~TemporalIntervalView() override;

  QRectF boundingRect() const override;

  const TemporalIntervalPresenter& presenter() const;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void setExecutionDuration(const TimeVal& progress);

  void updateOverlayPos();
  void setSelected(bool selected);

public:
  void intervalHoverEnter() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalHoverEnter)
  void intervalHoverLeave() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalHoverLeave)

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

  void updatePaths() final override;
  void updatePlayPaths() final override;
  void drawDashedPath(QPainter& p, QRectF visibleRect, const Process::Style& skin);
  void drawPlayDashedPath(QPainter& p, QRectF visibleRect, const Process::Style& skin);
};
}
