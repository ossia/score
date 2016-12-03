#pragma once
#include <QGraphicsItem>
#include <QPen>
#include <QRect>
#include <QtGlobal>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <iscore_plugin_scenario_export.h>
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class SlotView;

class ISCORE_PLUGIN_SCENARIO_EXPORT SlotHandle final : public QGraphicsItem
{
public:
  SlotHandle(const SlotView& slotView, QGraphicsItem* parent);

  static constexpr int static_type()
  {
    return QGraphicsItem::UserType + ItemType::SlotHandle;
  }
  int type() const override
  {
    return static_type();
  }

  static constexpr double handleHeight()
  {
    return 3.;
  }

  const SlotView& slotView() const
  {
    return m_slotView;
  }

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setWidth(qreal width);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

  const SlotView& m_slotView;
  qreal m_width{};
  QPen m_pen;
};
}
