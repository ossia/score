#pragma once
#include <QGraphicsItem>
#include <qnamespace.h>

#include <score_plugin_scenario_export.h>

class QGraphicsSceneMouseEvent;

namespace Scenario
{
class IntervalView;

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalBrace : public QGraphicsItem
{
public:
  IntervalBrace(const IntervalView& parentCstr, QGraphicsItem* parent);

  QRectF boundingRect() const override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

protected:
  const IntervalView& m_parent;
  QPainterPath m_path;

private:
};
}
