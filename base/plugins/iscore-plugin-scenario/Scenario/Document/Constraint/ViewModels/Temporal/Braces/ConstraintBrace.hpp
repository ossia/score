#pragma once
#include <QBrush>
#include <iscore/tools/GraphicsItem.hpp>
#include <iscore_plugin_scenario_export.h>
#include <qnamespace.h>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>

class QGraphicsSceneMouseEvent;

namespace Scenario
{

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintBrace : public GraphicsItem
{
public:
  ConstraintBrace(const ConstraintView& parentCstr, QQuickPaintedItem* parent);

  QRectF boundingRect() const override;

  void paint(
      QPainter* painter) override;

  void mousePressEvent(QMouseEvent* event) final override;
  void mouseMoveEvent(QMouseEvent* event) final override;
  void mouseReleaseEvent(QMouseEvent* event) final override;

protected:
  const ConstraintView& m_parent;
  QPainterPath m_path;

private:
};
}
