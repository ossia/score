#pragma once
#include <QQuickPaintedItem>
#include <QPen>
#include <QRect>
#include <QtGlobal>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <iscore_plugin_scenario_export.h>
class QPainter;

class QWidget;

namespace Scenario
{
class SlotView;

class ISCORE_PLUGIN_SCENARIO_EXPORT SlotHandle final : public QQuickPaintedItem
{
public:
  SlotHandle(const SlotView& slotView, QQuickPaintedItem* parent);

  static constexpr int static_type()
  {
    return 1337 + ItemType::SlotHandle;
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
      QPainter* painter) override;

  void setWidth(qreal width);

private:
  void mousePressEvent(QMouseEvent* event) final override;
  void mouseMoveEvent(QMouseEvent* event) final override;
  void mouseReleaseEvent(QMouseEvent* event) final override;

  const SlotView& m_slotView;
  qreal m_width{};
  QPen m_pen;
};
}
