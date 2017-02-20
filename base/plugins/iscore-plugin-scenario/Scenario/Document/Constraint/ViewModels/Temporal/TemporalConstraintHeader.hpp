#pragma once
#include <QQuickPaintedItem>
#include <QRect>
#include <QTextLayout>
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>
#include <qnamespace.h>

class QGraphicsSceneMouseEvent;
class QPainter;

class QWidget;

namespace Scenario
{
class TemporalConstraintHeader final : public ConstraintHeader
{
  Q_OBJECT
public:
  TemporalConstraintHeader();

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter) override;
signals:
  void doubleClicked();

  void constraintHoverEnter();
  void constraintHoverLeave();
  void dropReceived(const QPointF& pos, const QMimeData*);

  void shadowChanged(bool);

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
  void on_textChange() override;
  int m_previous_x{};

  int m_textWidthCache;
  // QTextLayout m_textCache;
};
}
