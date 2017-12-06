#pragma once
#include <QGraphicsSvgItem>
#include <QRect>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

class QGraphicsSceneMouseEvent;
class QGraphicsSvgItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class QMimeData;
namespace Scenario
{
class TriggerView final : public QGraphicsSvgItem
{
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

public:
  TriggerView(QGraphicsItem* parent);

  static constexpr int static_type()
  {
    return QGraphicsItem::UserType + ItemType::Trigger;
  }
  int type() const override
  {
    return static_type();
  }

signals:
  void pressed(QPointF);

  void dropReceived(const QPointF& pos, const QMimeData*);

protected:
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
};
}
