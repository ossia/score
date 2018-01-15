#pragma once
#include <Process/LayerView.hpp>
#include <score/tools/std/Optional.hpp>
#include <QPoint>
#include <QRect>

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
  Q_OBJECT

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
Q_SIGNALS:
  void clearPressed();
  void escPressed();

  void keyPressed(int);
  void keyReleased(int);

  // Screen pos, scene pos
  void dragEnter(const QPointF& pos, const QMimeData&);
  void dragMove(const QPointF& pos, const QMimeData&);
  void dragLeave(const QPointF& pos, const QMimeData&);

public Q_SLOTS:
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
    emit pressed(p);
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
  QRectF m_selectArea{};
  QPointF m_previousPoint{};
  ossia::optional<QRectF> m_dragLine{};
  bool m_lock{};
};
}
