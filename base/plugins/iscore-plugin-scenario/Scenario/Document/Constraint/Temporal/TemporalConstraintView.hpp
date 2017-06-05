#pragma once
#include <Process/TimeValue.hpp>
#include <QColor>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QtGlobal>
#include <Scenario/Document/CommentBlock/TextItem.hpp>
#include <Scenario/Document/Constraint/ExecutionState.hpp>
#include <Scenario/Document/Constraint/ConstraintView.hpp>
#include <iscore/model/ColorReference.hpp>

class QMimeData;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneDragDropEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class ConstraintMenuOverlay;
namespace Process { class LayerPresenter; }
namespace Scenario
{
class TemporalConstraintPresenter;
class ConstraintDurations;

class ISCORE_PLUGIN_SCENARIO_EXPORT TemporalConstraintView final
    : public ConstraintView
{
  Q_OBJECT

public:
  TemporalConstraintView(
      TemporalConstraintPresenter& presenter, QGraphicsItem* parent);

  QRectF boundingRect() const override;

  const TemporalConstraintPresenter& presenter() const;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void enableOverlay(bool b) override;

  void setLabelColor(iscore::ColorRef labelColor);
  void setLabel(const QString& label);

  void setColor(iscore::ColorRef c)
  {
    m_bgColor = std::move(c);
    update();
  }

  void setExecutionDuration(const TimeVal& progress);

signals:
  void constraintHoverEnter();
  void constraintHoverLeave();
  void dropReceived(const QPointF& pos, const QMimeData*);

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void updatePaths() final override;
  iscore::ColorRef m_bgColor;

  QPainterPath solidPath, dashedPath, playedSolidPath, playedDashedPath, waitingDashedPath;
};
}
