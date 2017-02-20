#pragma once
#include <QGraphicsSvgItem>
#include <QRect>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

class QGraphicsSceneMouseEvent;
class QGraphicsSvgItem;
class QPainter;

class QWidget;

namespace Scenario
{
class TriggerView final : public QGraphicsSvgItem
{
  Q_OBJECT
  

public:
  TriggerView(QQuickPaintedItem* parent);

  static constexpr int static_type()
  {
    return 1337 + ItemType::Trigger;
  }
  int type() const override
  {
    return static_type();
  }

signals:
  void pressed();

protected:
  void mousePressEvent(QMouseEvent*) override;

private:
  QGraphicsSvgItem* m_item{};
};
}
