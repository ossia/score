#pragma once
#include <Process/TimeValue.hpp>
#include <QColor>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QtGlobal>
#include <Scenario/Document/CommentBlock/TextItem.hpp>
#include <Scenario/Document/Interval/ExecutionState.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <score/model/ColorReference.hpp>
class QMimeData;
namespace Process { class LayerPresenter; }
namespace Scenario
{
class TemporalIntervalPresenter;
class IntervalDurations;
class SCORE_PLUGIN_SCENARIO_EXPORT TemporalIntervalView final
    : public IntervalView
{
  Q_OBJECT

public:
  TemporalIntervalView(
      TemporalIntervalPresenter& presenter, QGraphicsItem* parent);

  QRectF boundingRect() const override;

  const TemporalIntervalPresenter& presenter() const;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void enableOverlay(bool b);

  void setLabelColor(score::ColorRef labelColor);
  void setLabel(const QString& label);

  void setExecutionDuration(const TimeVal& progress);

  void updateOverlayPos();
  void setSelected(bool selected);
Q_SIGNALS:
  void intervalHoverEnter();
  void intervalHoverLeave();
  void dropReceived(const QPointF& pos, const QMimeData*);

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void updatePaths() final override;
  void updatePlayPaths() final override;

  QPainterPath solidPath, dashedPath, playedSolidPath, playedDashedPath, waitingDashedPath;

};
}
