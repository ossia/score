#pragma once
#include <Process/LayerView.hpp>

#include <score/tools/std/Optional.hpp>
#include <score/widgets/MimeData.hpp>

#include <QPoint>
#include <QRect>

#include <score_plugin_scenario_export.h>

#include <verdigris>
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
  ScenarioView(QGraphicsItem* parent);
  ~ScenarioView();
  void init(ScenarioPresenter* p) { m_scenario = p; }

  void paint_impl(QPainter* painter) const override;

  void setSelectionArea(const QRectF& rect)
  {
    m_selectArea = rect;
    update();
  }

  void drawDragLine(QPointF, QPointF, const QString&);
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

  void setSnapLine(std::optional<double>);

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
  ScenarioPresenter* m_scenario{};
  QRectF m_selectArea{};
  QPointF m_previousPoint{};
  std::optional<QRectF> m_dragLine{};
  std::optional<double> m_snapLine{};
  QString m_dragText;
  bool m_lock{};
  bool m_moving{};
};
}
