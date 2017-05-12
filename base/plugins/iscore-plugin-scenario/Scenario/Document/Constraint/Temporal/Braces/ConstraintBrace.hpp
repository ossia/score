#pragma once
#include <QBrush>
#include <QGraphicsItem>
#include <iscore_plugin_scenario_export.h>
#include <qnamespace.h>

class QGraphicsSceneMouseEvent;

namespace Scenario
{
class ConstraintView;

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintBrace : public QGraphicsItem
{
public:
  ConstraintBrace(const ConstraintView& parentCstr, QGraphicsItem* parent);

  QRectF boundingRect() const override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

protected:
  const ConstraintView& m_parent;
  QPainterPath m_path;

private:
};
}
