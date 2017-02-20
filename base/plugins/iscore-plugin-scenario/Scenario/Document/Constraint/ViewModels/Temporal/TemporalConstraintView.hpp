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
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <iscore/model/ColorReference.hpp>

class QMimeData;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneDragDropEvent;
class QPainter;

class QWidget;
class ConstraintMenuOverlay;
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
      TemporalConstraintPresenter& presenter, QQuickPaintedItem* parent);

  QRectF boundingRect() const override
  {
    qreal x = std::min(0., minWidth());
    qreal rectW = infinite() ? defaultWidth() : maxWidth();
    rectW -= x;
    return {x, -4, rectW, qreal(constraintAndRackHeight())};
  }

  void paint(
      QPainter* painter) override;

  void enableOverlay(bool b) override;

  void setLabelColor(iscore::ColorRef labelColor);
  void setLabel(const QString& label);

  void setColor(iscore::ColorRef c)
  {
    m_bgColor = c;
    update();
  }

  void setExecutionDuration(const TimeVal& progress);

signals:
  void constraintHoverEnter();
  void constraintHoverLeave();
  void dropReceived(const QPointF& pos, const QMimeData*);

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  iscore::ColorRef m_bgColor;
};
}
