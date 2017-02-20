#pragma once
#include <QQuickPaintedItem>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QtGlobal>
#include <Scenario/Document/VerticalExtent.hpp>
#include <iscore/model/ColorReference.hpp>

#include "ExecutionStatus.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <iscore_plugin_scenario_export.h>
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QMimeData;
class QPainter;

class QWidget;

namespace Scenario
{
class ConditionView;
class EventPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT EventView final : public QQuickPaintedItem
{
  Q_OBJECT


public:
  EventView(EventPresenter& presenter, QQuickPaintedItem* parent);
  virtual ~EventView() = default;

  static constexpr int static_type()
  {
    return 1337 + ItemType::Event;
  }
  int type() const override
  {
    return static_type();
  }

  const EventPresenter& presenter() const
  {
    return m_presenter;
  }

  QRectF boundingRect() const override
  {
    return {-3, -10., 6, qreal(m_extent.bottom() - m_extent.top() + 20)};
  }

  void paint(
      QPainter* painter) override;

  void setSelected(bool selected);
  bool isSelected() const;

  void setCondition(const QString& cond);
  bool hasCondition() const;

  void setTrigger(const QString& trig);
  bool hasTrigger() const;

  void setExtent(const VerticalExtent& extent);
  void setExtent(VerticalExtent&& extent);

  void setStatus(ExecutionStatus s);

  void changeColor(iscore::ColorRef);

  void setWidthScale(double);
  void changeToolTip(const QString&);

signals:
  void eventHoverEnter();
  void eventHoverLeave();

  void dropReceived(const QPointF& pos, const QMimeData*);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  EventPresenter& m_presenter;
  QString m_condition;
  QString m_trigger;
  iscore::ColorRef m_color;

  ExecutionStatusProperty m_status{};
  bool m_selected{};

  VerticalExtent m_extent;

  ConditionView* m_conditionItem{};
};
}
