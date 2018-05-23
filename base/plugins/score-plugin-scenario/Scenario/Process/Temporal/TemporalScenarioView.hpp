#pragma once
#include <Process/LayerView.hpp>
#include <wobjectdefs.h>
#include <QPoint>
#include <QRect>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MimeData.hpp>

class QGraphicsItem;
class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QMimeData;
class QPainter;

namespace Scenario
{
class TemporalScenarioPresenter;

class TemporalScenarioView final : public Process::LayerView
{
  W_OBJECT(TemporalScenarioView)

public:
  TemporalScenarioView(QGraphicsItem* parent);
  ~TemporalScenarioView();

  void paint_impl(QPainter* painter) const override;

  void setSelectionArea(const QRectF& rect)
  {
    m_selectArea = rect;
    update();
  }

  void drawDragLine(QPointF, QPointF);
  void stopDrawDragLine();
public:
  void clearPressed() W_SIGNAL(clearPressed);
  void escPressed() W_SIGNAL(escPressed);

  void keyPressed(int arg_1) W_SIGNAL(keyPressed, arg_1);
  void keyReleased(int arg_1) W_SIGNAL(keyReleased, arg_1);

  // Screen pos, scene pos
  void dragEnter(const QPointF& pos, const QMimeData& arg_2) W_SIGNAL(dragEnter, pos, arg_2);
  void dragMove(const QPointF& pos, const QMimeData& arg_2) W_SIGNAL(dragMove, pos, arg_2);
  void dragLeave(const QPointF& pos, const QMimeData& arg_2) W_SIGNAL(dragLeave, pos, arg_2);

public:
  void lock()
  {
    m_lock = true;
    update();
  }; W_SLOT(lock)
  void unlock()
  {
    m_lock = false;
    update();
  }; W_SLOT(unlock)

  void pressedAsked(const QPointF& p)
  {
    m_previousPoint = p;
    pressed(p);
  }; W_SLOT(pressedAsked)
  void movedAsked(const QPointF& p); W_SLOT(movedAsked);

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
  QRectF m_selectArea{};
  QPointF m_previousPoint{};
  ossia::optional<QRectF> m_dragLine{};
  bool m_lock{};
  bool m_moving{};
};
}
