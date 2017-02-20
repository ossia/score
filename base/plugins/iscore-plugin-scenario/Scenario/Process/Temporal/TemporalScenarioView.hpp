#pragma once
#include <Process/LayerView.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QPoint>
#include <QRect>

class QQuickPaintedItem;
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
  TemporalScenarioView(QQuickPaintedItem* parent);
  ~TemporalScenarioView();

  void paint_impl(QPainter* painter) const override;

  void setSelectionArea(const QRectF& rect)
  {
    m_selectArea = rect;
    update();
  }

  void setPresenter(TemporalScenarioPresenter* pres)
  {
    m_pres = pres;
  }

  void drawDragLine(QPointF, QPointF);
  void stopDrawDragLine();
signals:
  void pressed(QPointF);
  void released(QPointF);
  void moved(QPointF);
  void doubleClick(QPointF);

  void clearPressed();
  void escPressed();

  void keyPressed(int);
  void keyReleased(int);

  // Screen pos, scene pos
  void askContextMenu(const QPoint&, const QPointF&);
  void dragEnter(const QPointF& pos, const QMimeData*);
  void dragMove(const QPointF& pos, const QMimeData*);
  void dragLeave(const QPointF& pos, const QMimeData*);
  void dropReceived(const QPointF& pos, const QMimeData*);

public slots:
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
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  TemporalScenarioPresenter* m_pres{};
  QRectF m_selectArea{};
  QPointF m_previousPoint{};
  iscore::optional<QRectF> m_dragLine{};
  bool m_lock{};
};
}
