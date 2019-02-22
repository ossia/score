#pragma once
#include <Process/LayerView.hpp>

#include <score/tools/std/Optional.hpp>
#include <score/widgets/MimeData.hpp>

#include <QPoint>
#include <QRect>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>
class QGraphicsItem;
class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QMimeData;
class QPainter;

namespace Scenario
{
class ProcessModel;
class ScenarioPresenter;

class ScenarioView final : public Process::LayerView
{
public:
  ScenarioView(const ProcessModel& m, QGraphicsItem* parent);
  ~ScenarioView();

  void paint_impl(QPainter* painter) const override;

  void setSelectionArea(const QRectF& rect)
  {
    m_selectArea = rect;
    update();
  }

  void drawDragLine(QPointF, QPointF);
  void stopDrawDragLine();

public:
  void lock()
  {
    m_lock = true;
    update();
  }
  void unlock()
  {
    m_lock = false;
    update();
  }

  void pressedAsked(const QPointF& p)
  {
    m_previousPoint = p;
    pressed(p);
  }
  void movedAsked(const QPointF& p);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  const ProcessModel& m_scenario;
  QRectF m_selectArea{};
  QPointF m_previousPoint{};
  ossia::optional<QRectF> m_dragLine{};
  bool m_lock{};
  bool m_moving{};
};
}
